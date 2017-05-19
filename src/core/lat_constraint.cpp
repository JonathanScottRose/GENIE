#include "pch.h"
#include "graph.h"
#include "node.h"
#include "node_system.h"
#include "net_rs.h"
#include "net_topo.h"
#include "port_rs.h"
#include "flow.h"
#include "lp_lib.h"

using namespace genie::impl;
using namespace flow;
using namespace graph;
using Dir = genie::Port::Dir;

namespace
{
	// LPSolve-specific versions of constraint and objective
	struct LPConstraint
	{
		std::vector<double> coefs;
		std::vector<int> varnos;
		int op;
		int rhs;
	};

	struct LPObjective
	{
		std::vector<double> coef;
		std::vector<int> varno;
		enum { MIN, MAX } direction;
	};

	// State of entire solver process
	struct SolverState
	{
		// System we're operating on
		NodeSystem* sys;

		// LPSolve constraints and objective
		std::vector<LPConstraint> lp_constraints;
		LPObjective lp_objective;

		// Forward and reverse mappings for:
		// latency variable# <-> external physical link ID
		//
		// latency variables are nonnegative integers that represent
		// the amount of latency cycles on each external physical link
		int next_varno_lat = 0;
		std::unordered_map<LinkID, int> link_to_varno_lat;
		std::unordered_map<int, LinkID> varno_lat_to_link;

		// Forward and reverse mappings for:
		// register variable# <-> edxternal physical link ID
		//
		// register variables are binary, and represent whether or not
		// an extenral physical link has ANY registering on it
		int next_varno_reg = 0;
		std::unordered_map<LinkID, int> link_to_varno_reg;
		std::unordered_map<int, LinkID> varno_reg_to_link;

		// The "reg graph".
		// Vertices represent external links
		// Edges represent combinational logic bridged by external links.
		// Edge weights represent the combinational logic depth.
		Graph reg_graph;
		E2Attr<unsigned> reg_graph_weights;
	};

	// Take a bag of (topo, rslogical) links.
	// Decompose into external + internal physical links
	// external ones: find/create var#
	// internal ones: find fixed latency and add to sum
	//
	// Internal links are found by looking at endpoints of
	// external links belonging to the bag of given E2E links.
	void process_e2e_links(SolverState& sstate, 
		const std::vector<genie::SyncConstraint::ChainTerm::Sign>& signs,
		const std::vector<LinkID>& e2e_links,
		std::vector<int>& out_varnos, 
		std::vector<double>& out_coefs,
		int& out_constant)
	{
		auto sys = sstate.sys;

		auto& link_rel = sys->get_link_relations();
		
		// Take all e2e links from the bag and decompose into external phys links
		std::unordered_map<LinkID, genie::SyncConstraint::ChainTerm::Sign> all_ext_phys;
		for (auto it = e2e_links.begin(); it != e2e_links.end(); ++it)
		{
			auto e2e_link = *it;
			auto sign = signs.at(it - e2e_links.begin());
		
			// Decompose the end-to-end link into physical links
			auto ext_phys = link_rel.get_children(e2e_link, NET_RS_PHYS);

			// Insert the physical links along with their +/- sign
			for (auto link : ext_phys)
			{
				all_ext_phys[link] = sign;
			}
		}

		// Get, or create, variables for all external phys links in the solver state
		for (auto it : all_ext_phys)
		{
			auto link = it.first;
			auto sign = it.second;

			unsigned varno = 0;

			auto it = sstate.link_to_varno_lat.find(link);
			if (it == sstate.link_to_varno_lat.end())
			{
				// No variable exists? Create a new one and assign it
				varno = sstate.next_varno_lat++;
				sstate.link_to_varno_lat[link] = varno;
				sstate.varno_lat_to_link[varno] = link;
			}
			else
			{
				varno = it->second;
			}

			// Return the variable number and its sign
			out_varnos.push_back(varno);
			out_coefs.push_back(sign == genie::SyncConstraint::ChainTerm::PLUS ?
				1.0 : -1.0);
		}

		// Find internal links that join the external links.
		// For each external link:
		// 1) gets its sink
		// 2) if it exists, traverse the internal link that begins at that sink
		// 3) find the external link on the other side, if it exists
		// 4) if that external link is in our ext_phys set, then use the
		// internal link's latency
		for (auto it : all_ext_phys)
		{
			auto link = it.first;
			auto sign = it.second;

			auto ext_sink = sys->get_link(link)->get_sink();
			auto int_src_ep = ext_sink->get_endpoint(NET_RS_PHYS, Dir::OUT);
			if (!int_src_ep)
				continue;

			// Look at internal link(s)
			for (auto int_link : int_src_ep->links())
			{
				auto int_sink_ep = int_link->get_sink_ep();
				auto other_phys = int_sink_ep->get_sibling()->get_link0();
				if (all_ext_phys.count(other_phys->get_id()))
				{
					// This internal link bridges two physical links in ext_phys.
					// This means we should consider its internal latency.

					int increment = (int)((LinkRSPhys*)(int_link))->get_latency();
					if (sign == genie::SyncConstraint::ChainTerm::MINUS)
						increment = -increment;

					out_constant += increment;
				}
			}
		}
	}

	void process_sync_constraints(SolverState& sstate)
	{
		using Op = genie::SyncConstraint::Op;

		auto sys = sstate.sys;
		auto& sync_constraints = sys->get_sync_constraints();

		for (auto& sync_constraint : sync_constraints)
		{
			std::vector<LinkID> log_link_ids;
			std::vector<genie::SyncConstraint::ChainTerm::Sign> signs;

			// Not every sync constraint applies to the domain being processed.
			// If a logical link named in the constraint doesn't seem to exist
			// in the system, we should skip the constraint.
			bool ignore_constraint = false;

			// Each chain term consists of a bunch of logical links and a sign.
			for (auto& chainterm : sync_constraint.chains)
			{
				// Decompose the chain into logical links.
				for (auto log_link : chainterm.links)
				{
					auto impl = dynamic_cast<LinkRSLogical*>(log_link);
					assert(impl);

					auto link_id = impl->get_id();
					if (sys->get_link(link_id) == nullptr)
					{
						ignore_constraint = true;
						break;
					}

					// Insert sign + logical link
					signs.push_back(chainterm.sign);
					log_link_ids.push_back(link_id);
				}

				if (ignore_constraint)
					break;
			}

			if (ignore_constraint)
				continue;

			// Create an LPSolve constraint
			sstate.lp_constraints.emplace_back();
			auto& lp_constraint = sstate.lp_constraints.back();

			// Convert (signs, logical link IDS) to (coefficient, variable #) and sum of
			// constant latencies
			int const_sum = 0;
			process_e2e_links(sstate, signs, log_link_ids, 
				lp_constraint.varnos, lp_constraint.coefs, const_sum);

			// Set the RHS constant
			lp_constraint.rhs = sync_constraint.rhs - const_sum;

			// Set the operator to match the user operator
			switch (sync_constraint.op)
			{
			case Op::LT: lp_constraint.rhs--;
			case Op::LE:
				lp_constraint.op = ROWTYPE_LE;
				break;
			case Op::EQ:
				lp_constraint.op = ROWTYPE_EQ;
				break;
			case Op::GT: lp_constraint.rhs++;
			case Op::GE:
				lp_constraint.op = ROWTYPE_GE;
				break;
			}
		}
	}

	void process_topo_constraints(SolverState& sstate)
	{
		using Op = genie::SyncConstraint::Op;
		using Sign = genie::SyncConstraint::ChainTerm::Sign;

		auto sys = sstate.sys;

		// Look at every topo link and check for min/max reg settings
		for (auto topo_link : sys->get_links_casted<LinkTopo>(NET_TOPO))
		{
			auto topo_min = topo_link->get_min_regs();
			auto topo_max = topo_link->get_max_regs();

			// Skip unconstrained topo links
			if (topo_min == 0 && topo_max == LinkTopo::REGS_UNLIMITED)
				continue;

			// Prepare variable #s and coefficients to represent physical links
			std::vector<int> varnos;
			std::vector<double> coefs;
			int const_sum = 0;

			// Process a single end-to-end link (this topo link) with a plus sense
			process_e2e_links(sstate, { Sign::PLUS }, { topo_link->get_id() },
				varnos, coefs, const_sum);

			// If min_regs was specified, create an LPSolve constraint
			if (topo_min > 0)
			{
				sstate.lp_constraints.emplace_back();
				auto& lp_constraint = sstate.lp_constraints.back();
				lp_constraint.coefs = coefs;
				lp_constraint.varnos = varnos;
				lp_constraint.op = ROWTYPE_GE;
				lp_constraint.rhs = (int)topo_min;
			}

			// If max_regs specified, create an LPSolve constraint
			if (topo_max != LinkTopo::REGS_UNLIMITED)
			{
				sstate.lp_constraints.emplace_back();
				auto& lp_constraint = sstate.lp_constraints.back();
				lp_constraint.coefs = coefs;
				lp_constraint.varnos = varnos;
				lp_constraint.op = ROWTYPE_LE;
				lp_constraint.rhs = (int)topo_max;
			}
		}
	}

	void create_reg_graph(SolverState& sstate)
	{
		using namespace graph;
		auto sys = sstate.sys;
		auto& reg_graph = sstate.reg_graph;
		
		E2Attr<Link*> g1_e2link;
		V2Attr<HierObject*> g1_v2port;
		Graph g1 = flow::net_to_graph(sys, NET_RS_PHYS, true,
			&g1_v2port, nullptr,
			&g1_e2link, nullptr);

		std::vector<LinkRSPhys*> int_links;

		// Create vertices in reg_graph for each external edge
		for (auto eid : g1.iter_edges)
		{
			// Make sure the link is external.
			// If internal, save for later
			Link* ext_link = g1_e2link[eid];
			LinkID ext_link_id = ext_link->get_id();
			bool is_external = ext_link == sys->get_link(ext_link_id);

			if (!is_external)
			{
				int_links.push_back(static_cast<LinkRSPhys*>(ext_link));
				continue;
			}

			// Create vertex
			reg_graph.newv((VertexID)ext_link_id);

			// Create variable
			int varno = sstate.next_varno_reg++;
			sstate.varno_reg_to_link[varno] = ext_link_id;
			sstate.link_to_varno_reg[ext_link_id] = varno;
		}

		// Tackle the internal links.
		// If internal link has >0 latency: ignore it.
		// If internal link has 0 latency: bridge
		for (auto int_link : int_links)
		{
			if (int_link->get_latency() > 0)
				continue;

			// Get source and sink of internal link, add up their logic depth.
			auto int_src = static_cast<PortRS*>(int_link->get_src());
			auto int_sink = static_cast<PortRS*>(int_link->get_sink());
			unsigned logic_depth = int_src->get_logic_depth() +
				int_sink->get_logic_depth();

			// Get the external links that the internal src/sink are connected to.
			// Find the vertex IDs representing those edges in the graph.
			// Connect them with an edge, with the edge weight being the logic depth.
			auto int_src_link = int_src->get_endpoint(NET_RS_PHYS, Dir::IN)->get_link0();
			auto int_sink_link = int_sink->get_endpoint(NET_RS_PHYS, Dir::OUT)->get_link0();

			// Link ID == vertex ID in reg_graph
			auto int_src_link_v = (VertexID)int_src_link->get_id();
			auto int_sink_link_v = (VertexID)int_sink_link->get_id();

			// Connect them
			EdgeID e = reg_graph.newe(int_src_link_v, int_sink_link_v);
			sstate.reg_graph_weights[e] = logic_depth;
		}

		// Handle "stub ports".
		// These are ports that are terminal srces/sinks in the physical graph.
		// They still contribute a logic depth.
		// Create vertices for them in the reg_graph, representing the registered core
		// of the module behind the stub port.
		for (auto port_v : g1.iter_verts)
		{
			auto port = static_cast<PortRS*>(g1_v2port[port_v]);
			bool term_src = !port->get_endpoint(NET_RS_PHYS, Port::Dir::IN)->is_connected();
			bool term_sink = !port->get_endpoint(NET_RS_PHYS, Port::Dir::OUT)->is_connected();
			
			// We only want ports that are terminal
			if (!term_src && !term_sink)
				continue;

			// Get external link connected to terminal port
			auto term_dir = term_src ? Port::Dir::OUT : Port::Dir::IN;
			auto ext_link = port->get_endpoint(NET_RS_PHYS, term_dir)->get_link0();
			
			// and its vertex in the reg graph
			VertexID ext_v = (VertexID)ext_link->get_id();

			// Create vertex in reg graph representing the terminal.
			// Its ID means nothing, unlike the other existing vertices whose IDs are link IDs
			VertexID term_v = reg_graph.newv();
			
			// Connect terminal v with extlink v and assign it logic depth.
			EdgeID e = reg_graph.newe(
				term_src ? term_v : ext_v,
				term_src ? ext_v : term_v);

			// Get logic depth
			unsigned logic_depth = port->get_logic_depth();
			sstate.reg_graph_weights[e] = logic_depth;
		}
	}
}

void flow::solve_latency_constraints(NodeSystem* sys)
{
	SolverState sstate;
	sstate.sys = sys;

	process_sync_constraints(sstate);
	process_topo_constraints(sstate);
	create_reg_graph(sstate);
}

/*
void flow::solve_latency_constraints(NodeSystem* sys)
{
    
    
    std::vector<RowConstraint> lp_constraints;

    // Holds mapping between free RVD links and column variables
    std::unordered_map<RVDLink*, int> link_to_varno;
    int next_varno = 1;

    // Process each constraint
    for (auto& constraint : aconstraints->constraints)
    {
        lp_constraints.resize(lp_constraints.size() + 1);

        // Current constraint
        RowConstraint& cur_row = lp_constraints.back();

        // Sum of constants on LHS of inequality
        int row_lhs = 0;

        // Process each path term in the constraint
        for (auto& path_term : constraint.path_terms)
        {
            auto path_sign = path_term.first;
            RSPath& path = path_term.second;

            // Used to chain each RS link's source with the sink of the previous
            RSPort* last_rs_sink = nullptr;

            // Walk each RS Link in the path
            for (auto& rs_link : path)
            {
                if (last_rs_sink)
                {
                    auto cur_rs_src = RSPort::get_rs_port(rs_link->get_src());

                    auto int_links = last_rs_sink->get_node()->get_links(last_rs_sink, cur_rs_src);

                    if (int_links.empty())
                    {
                        throw HierException(cur_rs_src->get_node(), 
                            "can't process latency constraint because there exists no internal RS Link "
                            "between '" + last_rs_sink->get_name() + "' and '" + cur_rs_src->get_name() + "'");
                    }

                    // Internal edges contribute a constant latency term to the LHS of the inequality
                    auto int_link = (RSLink*)int_links.front();
                    row_lhs += path_sign==RSLatencyConstraint::PLUS?
                        int_link->get_latency() : -int_link->get_latency();
                }

                // loop over child RVDEdges, get their latencies, add to constraint
                auto acont = rs_link->asp_get<ALinkContainment>();
                for (auto child_link : acont->get_all_child_links(NET_RVD))
                {
                    auto rvd_link = (RVDLink*)child_link;

                    // Internal link? Add latency to LHS. Otherwise, it becomes a variable to solve for
                    bool internal = rvd_link->get_src()->get_node() == rvd_link->get_sink()->get_node();
                    if (internal)
                    {
                        row_lhs += path_sign==RSLatencyConstraint::PLUS?
                            rvd_link->get_latency() : -rvd_link->get_latency();
                    }
                    else
                    {
                        // create/get 'column' variable
                        auto varit = link_to_varno.find(rvd_link);
                        if (varit == link_to_varno.end())
                            varit = link_to_varno.emplace(rvd_link, next_varno++).first;
                        int varno = varit->second;

                        cur_row.coef.resize(next_varno-1);
                        cur_row.varno.resize(next_varno-1);

                        // add coefficient (+1/-1) to the row
                        cur_row.coef[varno-1] += path_sign == RSLatencyConstraint::PLUS? 1.0 : -1.0;
                        cur_row.varno[varno-1] = varno;
                    }
                }

                last_rs_sink = RSPort::get_rs_port(rs_link->get_sink());
            }
        } // end path terms

        // LHS is done. Set up row's comparison function and RHS
        cur_row.rhs = constraint.rhs;
        cur_row.rhs -= row_lhs;
        
        switch (constraint.op)
        {
        case RSLatencyConstraint::LT:
            cur_row.rhs--;
        case RSLatencyConstraint::LEQ:
            cur_row.op = ROWTYPE_LE;
            break;
        case RSLatencyConstraint::CompareOp::EQ:
            cur_row.op = ROWTYPE_EQ;
            break;
        case RSLatencyConstraint::GT:
            cur_row.rhs++;
        case RSLatencyConstraint::GEQ:
            cur_row.op = ROWTYPE_GE;
            break;
        }
    }// for each constraint

     // Set up lp_solve problem
    lprec* lp_prob = make_lp(lp_constraints.size(), next_varno-1);
    if (!lp_prob)
        throw HierException(sys, "couldn't create lp_solve problem");

    set_verbose(lp_prob, NEUTRAL);
    put_logfunc(lp_prob, nullptr, nullptr);
    put_msgfunc(lp_prob, nullptr, nullptr, -1);
    set_presolve(lp_prob, PRESOLVE_ROWS | PRESOLVE_COLS, get_presolveloops(lp_prob));
    set_add_rowmode(lp_prob, TRUE);

    // Set the objective function to minimize
    {
        std::vector<double> cns_coef;
        std::vector<int> cns_varno;

        for (auto it : link_to_varno)
        {
            RVDLink* link = it.first;
            int varno = it.second;

            // Coefficient = width of RVD link in bits.
            // Add 1 to width to include overhead of valid bits, etc
            int cost = link->get_width() + 1;

            cns_coef.push_back((double)cost);
            cns_varno.push_back(varno);
        }

        if (!set_obj_fnex(lp_prob, cns_coef.size(), cns_coef.data(), cns_varno.data()))
            throw HierException(sys, "failed setting objective function");
    }

    // Register all constraints
    for (auto& row : lp_constraints)
    {
        if (!add_constraintex(lp_prob, row.coef.size(), row.coef.data(), row.varno.data(), 
            row.op, (double)row.rhs))
        {
            throw HierException(sys, "failed adding row constraint");
        }
    }

    // Make all variables integers
    set_add_rowmode(lp_prob, FALSE);
    for (int i = 1; i < next_varno; i++)
    {
        set_int(lp_prob, i, TRUE);
    }

    // Solve and get results
    set_minim(lp_prob);
    switch (solve(lp_prob))
    {
    case NOMEMORY: throw HierException(sys, "lpsolve: out of memory"); break;
    case NOFEASFOUND:
    case INFEASIBLE: throw HierException(sys, "lpsolve: infeasible solution"); break;
    case DEGENERATE:
    case NUMFAILURE:
    case PROCFAIL: throw HierException(sys, "lpsolve: misc failure"); break;
    default: break;
    }

    // Get results and set latencies
    {
        int nrows = get_Norig_rows(lp_prob);
        int ncols = get_Norig_columns(lp_prob);

        auto varno_to_link = util::reverse_map(link_to_varno);

        for (int col = 1; col <= ncols; col++)
        {
            int latency = (int)get_var_primalresult(lp_prob, nrows + col);

            RVDLink* link = varno_to_link[col].front();
            link->set_latency(latency);
        }
    }

    delete_lp(lp_prob);
}
*/