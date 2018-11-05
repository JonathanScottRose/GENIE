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

		// Next LPSolve variable index. Can not be zero, so start at 1.
		// Also, keep track of which variables are latency related or
		// register related.
		int next_varno = 1;
		std::vector<int> varno_lat;
		std::vector<int> varno_reg;

		// Forward and reverse mappings for:
		// latency variable# <-> external physical link ID
		//
		// latency variables are nonnegative integers that represent
		// the amount of latency cycles on each external physical link
		std::unordered_map<LinkID, int> link_to_varno_lat;
		std::unordered_map<int, LinkID> varno_lat_to_link;

		// Forward and reverse mappings for:
		// register variable# <-> edxternal physical link ID
		//
		// register variables are binary, and represent whether or not
		// an extenral physical link has ANY registering on it
		std::unordered_map<LinkID, int> link_to_varno_reg;
		std::unordered_map<int, LinkID> varno_reg_to_link;

		// The "reg graph".
		// Vertices represent external links
		// Edges represent combinational logic bridged by external links.
		// Edge weights represent the combinational logic depth.
		Graph reg_graph;
		E2Attr<unsigned> reg_graph_weights;
	};

	int get_or_create_lat_var(SolverState& sstate, LinkID link)
	{
		auto it = sstate.link_to_varno_lat.find(link);
		if (it == sstate.link_to_varno_lat.end())
		{
			int result = sstate.next_varno++;
			sstate.varno_lat.push_back(result);
			sstate.link_to_varno_lat[link] = result;
			sstate.varno_lat_to_link[result] = link;
			return result;
		}
		else
		{
			return it->second;
		}
	}

	// In addition to creating a reg variable and associating it
	// with an external link, it will do the following if creating a reg variable:
	//   1) get or create the LATENCY variable associated with the same link
	//   2) create an aux constraint that binds the new reg var and new/existing lat var
	int get_or_create_reg_var(SolverState& sstate, LinkID link)
	{
		auto it = sstate.link_to_varno_reg.find(link);
		if (it == sstate.link_to_varno_reg.end())
		{
			// Creating a reg variable! We will also tie this new
			// reg variable with the associated lat variable for the same link
			// with a special constraint.
			int varno_lat = get_or_create_lat_var(sstate, link);
			
			int varno_reg = sstate.next_varno++;
			sstate.varno_reg.push_back(varno_reg);
			sstate.link_to_varno_reg[link] = varno_reg;
			sstate.varno_reg_to_link[varno_reg] = link;

			// Create constraint: var_lat >= var_reg
			sstate.lp_constraints.emplace_back();
			auto& cns = sstate.lp_constraints.back();

			cns.coefs.insert(cns.coefs.end(), { 1, -1 });
			cns.varnos.insert(cns.varnos.end(), { varno_lat, varno_reg });
			cns.op = ROWTYPE_GE;
			cns.rhs = 0;

			return varno_reg;
		}
		else
		{
			return it->second;
		}
	}

	void create_forced_nonzero_lat_constraint(SolverState& sstate, int varno)
	{
		sstate.lp_constraints.emplace_back();
		LPConstraint& cns = sstate.lp_constraints.back();

		// 1*var >= 1
		cns.coefs.push_back(1);
		cns.varnos.push_back(varno);
		cns.op = ROWTYPE_GE;
		cns.rhs = 1;
	}

	// Processes a set of end-to-end (logical, topo) links.
	// Takes each one, decomposes it into physical links, and
	// generates lpsolve coefficients and a constant term.
	// These are passed by reference and can be accumulated-over
	// by multiple calls to this function.
	void process_e2e_links(SolverState& sstate,
		const std::vector<LinkID>& e2e_links,
		genie::SyncConstraint::ChainTerm::Sign chain_sign,
		std::unordered_map<LinkID, int>& out_link_coefs,
		int& out_const_sum)
	{
		auto sys = sstate.sys;
		auto& link_rel = sys->get_link_relations();

		// These remember the sources and sinks of every external physical link
		// touched while decomposing e2e links into physical links.
		// Will be used to handle internal links after handling all external links.
		std::unordered_set<HierObject*> phys_srcs, phys_sinks;

		// Walk through all e2e links composing the chain and decompose each
		// into a set of physical links.
		for (auto e2e_link : e2e_links)
		{
			// Decompose the end-to-end link into physical links
			auto phys_links = link_rel.get_children(e2e_link, NET_RS_PHYS);

			// Walk through the physical links
			for (auto phys_link_id : phys_links)
			{
				// Each physical link contributes either a +1 or -1 coefficient depending
				// on the sign of the chain term it's part of.
				// Accumulate these.
				out_link_coefs[phys_link_id] +=
					chain_sign == genie::SyncConstraint::ChainTerm::PLUS ?
					1 : -1;

				// Remember the endpoints.
				auto phys_link = sys->get_link(phys_link_id);
				phys_srcs.insert(phys_link->get_src());
				phys_sinks.insert(phys_link->get_sink());
			}
		} // end external links

		// Next part handles internal links
		for (auto phys_sink : phys_sinks)
		{
			// Grab the OUT endpoint of this sink, which should be the source
			// of any internal link within a module.
			auto int_src_ep = phys_sink->get_endpoint(NET_RS_PHYS, Port::Dir::OUT);

			// If there are any internal links, follow them.
			for (auto int_link : int_src_ep->links())
			{
				// Here is the other side of the internal link
				auto int_sink_port = int_link->get_sink();

				// If the other side of this internal link is actually
				// the source port of a physical link we've traversed in this chain,
				// then it's part of a contiguous path, and we should include
				// the link's internal latency in the accumulated constant that's
				// supposed to be updated by this function.

				if (phys_srcs.count(int_sink_port) > 0)
				{
					// This internal link bridges two physical links in ext_phys.
					// This means we should consider its internal latency.
					int increment = (int)((LinkRSPhys*)(int_link))->get_latency();
					if (chain_sign == genie::SyncConstraint::ChainTerm::MINUS)
						increment = -increment;

					// This accumulates into the constant term
					out_const_sum += increment;
				}
			}
		} // end internal links
	}

	void process_sync_constraints(SolverState& sstate)
	{
		using Op = genie::SyncConstraint::Op;

		auto sys = sstate.sys;
		auto& sync_constraints = sys->get_spec().sync_constraints;

		for (auto& sync_constraint : sync_constraints)
		{
			// Not every sync constraint applies to the domain being processed.
			// If a logical link named in the constraint doesn't seem to exist
			// in the system, we should skip the constraint.
			bool ignore_constraint = false;

			// Holds coefficients for each physical link term.
			// Canonically these are all initially zero,
			// and then each processed chain will accumulate coefficients
			// into here.
			std::unordered_map<LinkID, int> link_2_coef;

			// The accumulation of all constant terms from each chain.
			// Also modified during each pass of the chainterm loop.
			int const_sum = 0;

			// Process each chain term in the constraint
			for (auto& chainterm : sync_constraint.chains)
			{
				// Gather chainterm's logical links into this set.
				// Basically gonna just be a copy.
				std::vector<LinkID> e2e_links;

				for (auto log_link_generic : chainterm.links)
				{
					auto log_link = dynamic_cast<LinkRSLogical*>(log_link_generic);
					assert(log_link);

					// Log_link points to a link in the original system, but not
					// necessarily in the current domain.
					auto log_link_id = log_link->get_id();
					if (sys->get_link(log_link_id) == nullptr)
					{
						ignore_constraint = true;
						break;
					}

					e2e_links.push_back(log_link_id);
				}

				if (ignore_constraint)
					break;

				// Accumulate coefficients+constants for this chain
				process_e2e_links(sstate, e2e_links, chainterm.sign, link_2_coef, const_sum);
			}

			// If any logical link doesn't exist in this domain, ignore this
			// constraint altogether.
			// TODO: this can be optimized somehow
			if (ignore_constraint)
				continue;

			// Create an LPSolve constraint
			sstate.lp_constraints.emplace_back();
			auto& lp_constraint = sstate.lp_constraints.back();

			// Convert link_2_coef and into LPSolve variable numbers and coefficients.
			for (auto it : link_2_coef)
			{
				auto link = it.first;
				auto coef = it.second;
				
				// Get/create a latency-type variable.
				// This is an internal association for us between the LPSolve variable,
				// and the physical link that it represents.
				int varno = get_or_create_lat_var(sstate, link);

				// Add variable and its coefficient to the lpsolve constraint
				lp_constraint.varnos.push_back(varno);
				lp_constraint.coefs.push_back((double)coef);
			}

			// Set the RHS constant of the lpsolve constraint, using the user-provided constant
			// and the accumnulation of internal link latencies stored in const_sum
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

			// Break down just this topo link into a physical link and
			// generate coefficients+constants for lp problem
			std::unordered_map<LinkID, int> phys_2_coef;
			int const_sum;

			process_e2e_links(sstate, { topo_link->get_id() }, 
				Sign::PLUS, phys_2_coef, const_sum);

			// Convert phys_2_coef into lpsolve variable numbers+coefficients
			std::vector<int> varnos;
			std::vector<double> coefs;

			// Convert link_2_coef and into LPSolve variable numbers and coefficients.
			for (auto it : phys_2_coef)
			{
				auto link = it.first;
				auto coef = it.second;

				// Get/create a latency-type variable.
				// This is an internal association for us between the LPSolve variable,
				// and the physical link that it represents.
				int varno = get_or_create_lat_var(sstate, link);

				varnos.push_back(varno);
				coefs.push_back((double)coef);
			}

			// If min_regs was specified, create an LPSolve constraint
			if (topo_min > 0)
			{
				sstate.lp_constraints.emplace_back();
				auto& lp_constraint = sstate.lp_constraints.back();
				lp_constraint.coefs = coefs;
				lp_constraint.varnos = varnos;
				lp_constraint.op = ROWTYPE_GE;
				lp_constraint.rhs = (int)topo_min - const_sum;
			}

			// If max_regs specified, create an LPSolve constraint
			if (topo_max != LinkTopo::REGS_UNLIMITED)
			{
				sstate.lp_constraints.emplace_back();
				auto& lp_constraint = sstate.lp_constraints.back();
				lp_constraint.coefs = coefs;
				lp_constraint.varnos = varnos;
				lp_constraint.op = ROWTYPE_LE;
				lp_constraint.rhs = (int)topo_max - const_sum;
			}
		}
	}

	void create_reg_graph(SolverState& sstate)
	{
		using namespace graph;
		auto sys = sstate.sys;
		unsigned max_logic_depth = sys->get_spec().max_logic_depth;
		auto& reg_graph = sstate.reg_graph;

		E2Attr<Link*> g1_e2link;
		V2Attr<HierObject*> g1_v2port;
		Graph g1 = flow::net_to_graph(sys, NET_RS_PHYS, true,
			&g1_v2port, nullptr,
			&g1_e2link, nullptr);;

		std::vector<LinkRSPhys*> int_links;

		// Go through each physical link. Each can be an external or internal link.
		// If internal: remember it for next step.
		// If external:
		//    - create a vertex for it in the reg graph
		//    - the src/sink of this external link belong to modules...
		//      create vertices representing the registered cores of those modules,
		//      and create edges annotated with the ports' internal combinational depths
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

			// Create vertex for external edge
			VertexID ext_v = (VertexID)ext_link_id;
			reg_graph.newv(ext_v);

			// Get the source and sink
			auto src_port = static_cast<PortRS*>(ext_link->get_src());
			auto sink_port = static_cast<PortRS*>(ext_link->get_sink());

			for (auto port : { src_port, sink_port })
			{
				// Get logic depth of the PORT - this is the number of LUTs
				// from the port to a register inside the module (worst-cased).
				unsigned logic_depth = port->get_logic_depth();

				// If logic depth >= max_logic_depth, then this external link must
				// be registered no matter what.
				// If this is the case:
				// Force the latency to be >= 1 of the external link (no need for involving binary variables)
				// Don't even bother to create a new vertex/edge in the reg graph.

				if (logic_depth >= max_logic_depth)
				{
					int varno_lat = get_or_create_lat_var(sstate, ext_link_id);
					create_forced_nonzero_lat_constraint(sstate, varno_lat);
				}
				else if (logic_depth == 0)
				{
					// do nothing, create nothing
				}
				else
				{
					// Create vertex in reg graph representing the terminal.
					// Its ID means nothing, unlike the other existing vertices whose IDs are link IDs
					VertexID term_v = reg_graph.newv();

					// Connect terminal v with extlink v and assign it logic depth.
					bool term_is_src = port == src_port;
					EdgeID e = reg_graph.newe(
						term_is_src ? term_v : ext_v,
						term_is_src ? ext_v : term_v);

					sstate.reg_graph_weights[e] = logic_depth;
				}
			}
		}

		// Tackle the internal links.
		// An internal physical link can have a combinational logic depth THROUGH
		// a module. This happens if the latency is 0.
		// If internal link has >0 latency: ignore it.
		// If internal link has 0 latency: bridge external links with an edge in reg graph
		for (auto int_link : int_links)
		{
			if (int_link->get_latency() > 0)
				continue;

			// Get logic depth on internal link			
			unsigned logic_depth = int_link->get_logic_depth();

			// Get endpoints of internal link
			auto int_src = int_link->get_src();
			auto int_sink = int_link->get_sink();

			// Get the external links that the internal src/sink are connected to.
			// Find the vertex IDs representing those edges in the graph.
			// Connect them with an edge, with the edge weight being the logic depth.
			auto int_src_link = int_src->get_endpoint(NET_RS_PHYS, Dir::IN)->get_link0();
			auto int_sink_link = int_sink->get_endpoint(NET_RS_PHYS, Dir::OUT)->get_link0();

			// Link ID == vertex ID in reg_graph
			auto int_src_link_v = (VertexID)int_src_link->get_id();
			auto int_sink_link_v = (VertexID)int_sink_link->get_id();

			// Connect the reg graph vertices. We need to solve for their 0/1ness.
			// The LP variables for them will be created later.
			EdgeID e = reg_graph.newe(int_src_link_v, int_sink_link_v);
			sstate.reg_graph_weights[e] = logic_depth;
		}
	}

	void postprocess_reg_graph(SolverState& sstate)
	{
		using namespace graph;
		auto sys = sstate.sys;
		auto& reg_graph = sstate.reg_graph;

		// Get a copy of all the edge IDs
		auto edges = reg_graph.edges();

		// Remove zero-weight edges from the graph.
		// This is an optimization.
		for (auto e : edges)
		{
			unsigned weight = sstate.reg_graph_weights[e];
			if (weight == 0)
			{
				auto verts = reg_graph.verts(e);
				reg_graph.mergev(verts.first, verts.second);
			}
		}
	}

	void create_reg_constraints(SolverState& sstate)
	{
		unsigned max_weight = sstate.sys->get_spec().max_logic_depth;
		auto& reg_graph = sstate.reg_graph;

		struct SnakeState
		{
			std::deque<VertexID> verts;
			unsigned total_weight;
			unsigned unvisited;
		};

		std::unordered_set<VertexID> visited;
		std::queue<SnakeState> snakes;

		// Generate initial snakes from terminal src vertices in reg graph
		for (auto v : reg_graph.iter_verts)
		{
			// No backwards edges? We want it.
			if (reg_graph.dir_neigh_r(v).empty())
			{
				snakes.emplace();
				auto& newsnake = snakes.back();
				newsnake.verts.push_back(v);
				newsnake.unvisited = 0;
				newsnake.total_weight = 0;
			}
		}

		// Process snakes
		while (!snakes.empty())
		{
			auto& cur_snake = snakes.front();
			bool snake_done = false;

			while (!snake_done)
			{
				VertexID cur_head = cur_snake.verts.back();

				// Assume the snake head has just been newly advanced into.
				// Check if it's been visited or not, and add to unvisited count if so
				if (!visited.count(cur_head))
					cur_snake.unvisited++;

				// See if the total snake weight violates the constraint
				if (cur_snake.total_weight > max_weight)
				{
					// Advance snake tail until snake weight is under the constraint
					// again.
					// Make sure to keep a snake that has at least 2 vertices in it
					// so that head != tail.

					while (cur_snake.total_weight > max_weight &&
						cur_snake.verts.size() > 2)
					{
						// Advance tail.
						VertexID old_tail = cur_snake.verts.front();
						cur_snake.verts.pop_front();
						VertexID new_tail = cur_snake.verts.front();

						// Mark outgoing tail as visited, update unvisited count
						auto inserted = visited.insert(old_tail);
						if (inserted.second)
						{
							// If inserted.second is true, then the visited set didn't have
							// this vertex before, meaning it was previously unvisited before
							// being marked visited. There is now one less unvisited vertex in
							// the body of the snake.
							cur_snake.unvisited--;
							// If it was already visited before, inserting into the set
							// is a safe null op
						}

						// Get the edge between the old tail and new tail.
						// Its weight will be subtracted from the weight of the snake.
						EdgeID e = reg_graph.edge(old_tail, new_tail);
						assert(e != INVALID_E);
						cur_snake.total_weight -= sstate.reg_graph_weights[e];

					};

					// If advancing the tail left us with no unvisited vertices left
					// in the snake, we can finish.
					if (cur_snake.unvisited == 0)
					{
						snake_done = true;
						break;
					}

					// Cur snake status: non-zero length by construction and
					// total weight less than max total weight. Has unvisited vertices.
					// Emit a constraint for the reg variables that each snake
					// vertex represents.
					// Use only vertices in the range [tail, head) of the snake.
					// Canonically, it should be (tail, head) on a just-overweight snake 
					// but we moved the tail vertex forwards an extra step as an optimization to
					// avoid having to do it once more after constraint emission.

					// Constraint:
					// 1*y0 + 1*y1 + 1*y2 + ... >= 1
					// where yi are binary variables that say whether or not at least one
					// register is needed on the corresponding external physical link.

					LPConstraint lp_constraint;

					for (auto it = cur_snake.verts.begin(); it < cur_snake.verts.end() - 1; ++it)
					{
						LinkID link_id = (LinkID)*it;
						auto varno = get_or_create_reg_var(sstate, link_id);

						lp_constraint.coefs.push_back(1);
						lp_constraint.varnos.push_back(varno);
					}

					lp_constraint.op = ROWTYPE_GE;
					lp_constraint.rhs = 1;
					sstate.lp_constraints.push_back(lp_constraint);
				} // if cur snake weight >= max snake weight

				// Get the set of next vertices that the snake head can expand into.
				// 3 possibilities:
				// 1) set is empty: means we're at a terminal sink vertex. mark all remaining
				//    snake vertices as visited and finish.
				// 2) set has one member: extend the snake head to become this
				// 3) set has >1 member: extend the snake head to become the first possiblity,
				//    and create new snakes for the others (push onto stack for later)

				auto next_vs = reg_graph.dir_neigh(cur_head);
				if (next_vs.empty())
				{
					// Nowhere for head to go: snake done, everyone'v visited, go home
					for (auto v : cur_snake.verts)
						visited.insert(v);
					snake_done = true;
				}
				else
				{
					// Go backwards through possibilities, such that the first candidate
					// is done last. This ensures that _this_ snake is modified last,
					// and pristine copies can be made beforehand.
					for (auto it = next_vs.rbegin(); it != next_vs.rend(); ++it)
					{
						// Other vertices: make a copy of current snake and extend it.
						// First vertex: extend current snake (this must be done last)
						auto snake = &cur_snake;
						if (it != next_vs.rend()-1 )
						{
							snakes.emplace(cur_snake); // copy unmodified snake
							snake = &snakes.back();
						}

						// The new head vertex
						auto new_head = *it;

						// Add weight of edge between cur_head and new_head to snake
						EdgeID e = reg_graph.edge(cur_head, new_head);
						snake->total_weight += sstate.reg_graph_weights[e];

						// Extend the snake forward making new_head the new head
						snake->verts.push_back(new_head);
					}
				} // snake head does/doesn't have forward neighbours
			} // while !snake_done

			snakes.pop();
		} // while !snakes.empty()
	}

	void solve_lp_constraints(SolverState& sstate)
	{
		auto& lp_constraints = sstate.lp_constraints;

		// Create lpsolve problem
		// #rows = # of constraints
		// #cols = # of variables (next_
		int n_rows = (int)lp_constraints.size();
		int n_cols = (int)sstate.varno_lat.size() + (int)sstate.varno_reg.size();

		// Sometimes, there's just nothing to do
		if (n_rows == 0 || n_cols == 0)
			return;

		lprec* lp_prob = make_lp(n_rows, n_cols);
		assert(lp_prob);
		// Set up LPsolve stuff. Keep it quiet wrt stdout
		set_verbose(lp_prob, NEUTRAL); // set_verbose(lp_prob, NEUTRAL);
		set_outputfile(lp_prob, "");
		
		// Presolve makes it faster?
		set_presolve(lp_prob, PRESOLVE_ROWS | PRESOLVE_COLS, get_presolveloops(lp_prob));
		
		// Transcribe the objective function
		{
			auto& obj = sstate.lp_objective;
			auto result = set_obj_fnex(lp_prob, obj.coef.size(), obj.coef.data(),
				obj.varno.data());
			assert(result);

			switch (obj.direction)
			{
			case LPObjective::MAX:
				set_maxim(lp_prob);
				break;
			case LPObjective::MIN:
				set_minim(lp_prob);
				break;
			}
		}

		// Transcribe all constraints
		// Rowmode means we add one constraint (row) at a time. Makes it faster.
		// Rowmode must be on only when entering constraints. Also, the objective
		// function must be specified before rowmode (and therefore also constraints).
		set_add_rowmode(lp_prob, TRUE);
		for (auto& row : lp_constraints)
		{
			auto result = add_constraintex(lp_prob, row.coefs.size(), row.coefs.data(),
				row.varnos.data(), row.op, (double)row.rhs);
			assert(result);
		}
		set_add_rowmode(lp_prob, FALSE);

		// Make the latency variables integers
		for (auto varno : sstate.varno_lat)
		{
			set_int(lp_prob, varno, TRUE);
		}

		// Make the reg variables binary
		for (auto varno : sstate.varno_reg)
		{
			set_binary(lp_prob, varno, TRUE);
		}

		// Solve and get results
		int solve_result = solve(lp_prob);
		switch (solve_result)
		{
		case NOMEMORY: 
		case INFEASIBLE: 
		case DEGENERATE:
		case NUMFAILURE:
		case PROCFAIL:
			assert(false);
		default: break;
		}

		// Use solutions of latency variables to set latency on physical links
		for (auto varno : sstate.varno_lat)
		{
			int onrows = get_Norig_rows(lp_prob);

			// Argument to get_var is an index into a thing that is laid out as:
			// 0 : objective function
			// [1, nrows] : values of constraints
			// [nrows + 1] : value of first variable (variable number 1)
			// Since variable numbers are 1-based, take that into account.
			int index = 1 + onrows + varno - 1;
			unsigned latency = (unsigned)get_var_primalresult(lp_prob, index);

			// Set the latency of the physical link
			LinkID link_id = sstate.varno_lat_to_link[varno];
			auto link = static_cast<LinkRSPhys*>(sstate.sys->get_link(link_id));
			link->set_latency(latency);
		}

		// Cleanup
		delete_lp(lp_prob);
	}

	void create_obj_func(SolverState& sstate)
	{
		// For each latency variable, get the associated physical link, and its width in bits.
		for (auto varno : sstate.varno_lat)
		{
			auto link_id = sstate.varno_lat_to_link[varno];
			auto link = sstate.sys->get_link(link_id);
			auto link_src = static_cast<PortRS*>(link->get_src());
			auto link_sink = static_cast<PortRS*>(link->get_sink());
			unsigned width = flow::calc_transmitted_width(link_src, link_sink);

			// Objective function:
			// minimize sum of: (coef)*varno
			// Where coef is the width in bits plus a constant offset of 1 to make sure
			// that represent the cost of even 0-width links (ready, valids).
			// TODO: instead of +1, actually look at whether link has valid/ready
			int coef = (int)width + 1;

			sstate.lp_objective.coef.push_back(coef);
			sstate.lp_objective.varno.push_back(varno);
		}

		// Set to minimize
		sstate.lp_objective.direction = LPObjective::MIN;
	}

	void dump_reg_graph(SolverState& sstate, unsigned dom_id)
	{
		if (sstate.lp_constraints.empty())
			return;

		auto vfunc = [&](VertexID v) -> std::string
		{
			// Get reg variable, if exists
			auto it_reg = sstate.link_to_varno_reg.find((LinkID)v);
			if (it_reg != sstate.link_to_varno_reg.end())
			{
				// Should have lat variable too
				auto it_lat = sstate.link_to_varno_lat.find((LinkID)v);
				assert(it_lat != sstate.link_to_varno_lat.end());

				return std::to_string(v) + "(R" + std::to_string(it_reg->second) +
					" L" + std::to_string(it_lat->second) + ")";
			}
			else
			{
				return std::to_string(v);
			}
		};

		auto efunc = [&](EdgeID e) -> std::string
		{
			auto w = sstate.reg_graph_weights[e];
			return std::to_string(w);
		};

		auto fname = util::str_con_cat(sstate.sys->get_name(),
			"reggraph", std::to_string(dom_id));

		sstate.reg_graph.dump(fname, vfunc, efunc);

		std::ofstream of(fname + "_lp.txt");
		for (auto& cns : sstate.lp_constraints)
		{
			for (unsigned i = 0; i < cns.coefs.size(); i++)
			{
				if (i > 0) of << " + ";
				of << cns.coefs[i] << "*" << cns.varnos[i];
			}

			switch (cns.op)
			{
			case ROWTYPE_LE: of << " <= "; break;
			case ROWTYPE_EQ: of << " = "; break;
			case ROWTYPE_GE: of << " >= "; break;
			default:assert(false);
			}

			of << cns.rhs << '\n';
		}

		of << ((sstate.lp_objective.direction == LPObjective::MIN) ?
			"MIN " : "MAX ");

		for (unsigned i = 0; i < sstate.lp_objective.coef.size(); i++)
		{
			if (i > 0) of << " + ";
			of << sstate.lp_objective.coef[i] << "*" << sstate.lp_objective.varno[i];
		}

		of.close();
		of.open(fname + "_edges.txt");
		for (auto vid : sstate.reg_graph.iter_verts)
		{
			auto link = sstate.sys->get_link((LinkID)vid);

			// Not all vertices represent links
			if (!link || link->get_type() != NET_RS_PHYS)
				continue;

			auto srcname = link->get_src()->get_hier_path(sstate.sys);
			auto sinkname = link->get_sink()->get_hier_path(sstate.sys);

			of << std::to_string(vid) + ": " + srcname + " -> " + sinkname << std::endl;
		}

		for (auto& entry : sstate.varno_lat_to_link)
		{
			auto link = sstate.sys->get_link(entry.second);
			auto srcname = link->get_src()->get_hier_path(sstate.sys);
			auto sinkname = link->get_sink()->get_hier_path(sstate.sys);
			of << "L" + std::to_string(entry.first) + ": " + srcname + " -> " + sinkname << std::endl;
		}
	}
}



void flow::solve_latency_constraints(NodeSystem* sys, unsigned dom_id)
{
	SolverState sstate;
	sstate.sys = sys;

	// Latency-related
	process_sync_constraints(sstate);
	process_topo_constraints(sstate);

	// Binary reg yes/no related
	create_reg_graph(sstate);
	//postprocess_reg_graph(sstate);
	create_reg_constraints(sstate);

	// Objective function
	create_obj_func(sstate);
	
	if (genie::impl::get_flow_options().dump_reggraph)
	{
		dump_reg_graph(sstate, dom_id);
	}

	// Solve and annotate latencies
	solve_lp_constraints(sstate);
}

