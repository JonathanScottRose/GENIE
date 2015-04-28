#include <unordered_set>
#include "genie/genie.h"
#include "genie/net_topo.h"
#include "genie/net_clock.h"
#include "genie/net_rvd.h"
#include "genie/node_merge.h"
#include "genie/node_split.h"
#include "genie/node_clockx.h"
#include "genie/lua/genie_lua.h"
#include "genie/graph.h"
#include "genie/value.h"
#include "genie/vlog.h"
#include "genie/vlog_bind.h"
#include "flow.h"
#include "genie/net_rs.h"
#include "globals.h"
#include "genie/node_flowconv.h"

using namespace genie;
using namespace genie::graphs;

namespace
{
	// Convert a network into a Graph
	typedef std::function<Port*(Port*)> N2GRemapFunc;
	Graph net_to_graph(System* sys, NetType ntype, 
		VAttr<Port*>* v_to_obj,
		RVAttr<Port*>* obj_to_v,
		EAttr<Link*>* e_to_link,
		REAttr<Link*>* link_to_e,
		const N2GRemapFunc& remap = N2GRemapFunc())
	{
		Graph result;

		// Gather all edges
		auto links = sys->get_links(ntype);

		// Whether or not the caller needs one, we require an obj->v map.
		// If the caller doesn't provide one, allocate (and then later free) a local one.
		bool local_map = !obj_to_v;
		if (local_map)
			obj_to_v = new RVAttr<Port*>;

		for (auto link : links)
		{
			// Get link's endpoints
			Port* src = link->get_src();
			Port* sink = link->get_sink();

			// Remap if a remap function was provided
			if (remap)
			{
				src = remap(src);
				sink = remap(sink);
			}

			// Create (or retrieve) associated vertex IDs for endpoints.
			// Update reverse association if caller requested one.
			VertexID src_v, sink_v;

			for (auto& p : {std::make_pair(&src_v, src), std::make_pair(&sink_v, sink)})
			{
				// src_v or sink_v
				VertexID* pv = p.first;
				// src or sink
				Port* obj = p.second;

				auto it = obj_to_v->find(obj);
				if (it == obj_to_v->end())
				{
					*pv = result.newv();
					obj_to_v->emplace(obj, *pv);
					if (v_to_obj) v_to_obj->emplace(*pv, obj);
				}
				else
				{
					*pv = it->second;
				}		
			}

			// Create edge and update mappings if requested
			EdgeID e = result.newe(src_v, sink_v);
			if (e_to_link) e_to_link->emplace(e, link);
			if (link_to_e) link_to_e->emplace(link, e);
		}

		if (local_map)
			delete obj_to_v;

		return result;
	}

	// Assign domains to RS Ports
	void rs_assign_domains(System* sys)
	{
		// Turn RS network into a graph.
		// Pass in a remap function which effectively treats all linkpoints as their parent ports.
		N2GRemapFunc remap = [=] (Port* o)
		{
			return RSPort::get_rs_port(o);
		};

		// We care about port->vid mapping and link->eid mapping
		REAttr<Link*> link_to_eid;
		RVAttr<Port*> port_to_vid;
		Graph rs_g = net_to_graph(sys, NET_RS, nullptr, &port_to_vid, nullptr, &link_to_eid, remap);

		// Identify connected components (domains).
		// Capture vid->domainid mapping and eid->domainid mapping
		VAttr<int> vid_to_domain;
		EAttr<int> eid_to_domain;
		connected_comp(rs_g, &vid_to_domain, &eid_to_domain);

		// Now assign each port its domain ID.
		for (auto& it : port_to_vid)
		{
			auto port = as_a<RSPort*>(it.first);
			assert(port);
			auto vid = it.second;
			
			// Look up the domain for this vid
			assert(vid_to_domain.count(vid));
			int domain = vid_to_domain[vid];

			port->set_domain_id(domain);
		}

		// Assign each RSLink its domain ID
		for (auto& it : link_to_eid)
		{
			auto link = as_a<RSLink*>(it.first);
			assert(link);
			auto eid = it.second;

			assert(eid_to_domain.count(eid));
			int domain = eid_to_domain[eid];

			link->set_domain_id(domain);
		}
	}

	// Assign Flow IDs to RS links
	void rs_assign_flow_ids(System* sys)
	{
		// Turn TOPO network into a graph. Merge together the input/output ports of split and merge
		// nodes for the purposes of graph construction.
		N2GRemapFunc remap = [] (Port* o) -> Port*
		{
			auto sp_node = as_a<NodeSplit*>(o->get_parent());
			if (sp_node) return sp_node->get_topo_input();
			
			auto mg_node = as_a<NodeMerge*>(o->get_parent());
			if (mg_node) return mg_node->get_topo_input();

			// Not an input/output port of a split/merge node. Do not remap.
			return o;
		};

		// Get TOPO network as graph. We need link->eid and HObj->vid
		EAttr<Link*> topo_eid_to_link;
		RVAttr<Port*> topo_port_to_vid;
		Graph topo_g = net_to_graph(sys, NET_TOPO, nullptr, &topo_port_to_vid, &topo_eid_to_link, nullptr,
			remap);

		// Split topo graph into connected domains
		EAttr<int> edge_to_comp;
		connected_comp(topo_g, nullptr, &edge_to_comp);

		auto comp_to_edges = util::reverse_map(edge_to_comp);

		for (auto& it : comp_to_edges)
		{
			// This is one domain

			// First, gather all the RS links carried by the TOPO links in this component.
			std::unordered_set<Link*> rs_links;
			for (auto& topo_edge : it.second)
			{
				Link* topo_link = topo_eid_to_link[topo_edge];
				auto ac = topo_link->asp_get<ALinkContainment>();
				auto parent_links = ac->get_all_parent_links(NET_RS);
				rs_links.insert(parent_links.begin(), parent_links.end());
			}

			// Next, gather all the sources of the RS links
			std::unordered_set<Endpoint*> rs_sources;
			for (Link* rs_link : rs_links)
			{
				auto src = rs_link->get_src_ep();
				assert(src);
				rs_sources.insert(src);
			}

			// Now start assigning Flow IDs to all the RS links emanating from each RS source.
			// IDs are assigned seqentially in a domain starting at 0.
			// All links coming from a single source (RSPort or RSLinkpoint) get the same ID
			Value flow_id = 0;
			for (auto rs_ep : rs_sources)
			{
				for (auto link : rs_ep->links())
				{
					auto rs_link = static_cast<RSLink*>(link);
					rs_link->set_flow_id(flow_id);
				}

				flow_id.set(flow_id + 1);
			}

			// Go back and calculate the width of all flow_ids in the domain, now that we know
			// the highest (final) value.
			int flow_id_width = std::max(1, util::log2(std::max(0, flow_id - 1)));
			for (auto link : rs_links)
			{
				auto rs_link = static_cast<RSLink*>(link);
				rs_link->get_flow_id().set_width(flow_id_width);
			}
		}

		/*
		topo_g.dump("flows", nullptr, [&](EdgeID eid)
		{
			// Topo domain.
			int domain = edge_to_comp[eid];
			std::string label = std::to_string(domain);

			label += ": ";

			// Append RS flow IDs
			auto link = topo_eid_to_link[eid];
			auto rs_links = link->asp_get<ALinkContainment>()->get_all_parent_links(NET_RS);
			for (auto& l : rs_links)
			{
				auto rsl = static_cast<RSLink*>(l);
				label += std::to_string(rsl->get_flow_id()) + ", ";
			}

			// Remove trailing comma
			label.pop_back();

			return label;
		});
		*/
	}


	void rs_refine_to_topo(System* sys)
	{
		// Create system contents ready for TOPO connectivity
		sys->refine(NET_TOPO);

		// Get the aspect that contains a reference to the System's Lua topology function
		auto atopo = sys->asp_get<ATopoFunc>();
		if (!atopo)
			throw HierException(sys, "no topology function defined for system");

		// Call topology function, passing the system as the first and only argument
		lua::push_ref(atopo->func_ref);
		lua::push_object(sys);
		lua::pcall_top(1, 0);

		// Unref topology function, and remove Aspect
		lua::free_ref(atopo->func_ref);
		sys->asp_remove<ATopoFunc>();
	}

	void topo_refine_to_rvd(System* sys)
	{
		// Refine all System contents such that they are RVD-connectable
		sys->refine(NET_RVD);

		// Go through all TOPO links in the system and create an RVD link for each.
		auto topo_links = sys->get_links(NET_TOPO);
		for (auto link : topo_links)
		{
			// These are the TOPO endpoints of the TOPO link
			auto src = as_a<TopoPort*>(link->get_src());
			auto sink = as_a<TopoPort*>(link->get_sink());
			assert(src);
			assert(sink);

			// We need to find corresponding RVD endpoints for the RVD link we're about to make.
			RVDPort* src_rvd = nullptr;
			RVDPort* sink_rvd = nullptr;

			// TOPO ports support multiple connections.
			// Each TOPO port contains an RVD port for each of those connections.
			// Here we find the first such unconnected RVD sub-port and use it as an endpoint
			// for our RVD link.
			for (int i = 0; i < src->get_n_rvd_ports(); i++)
			{
				src_rvd = src->get_rvd_port(i);
				if (!src_rvd->get_endpoint_sysface(NET_RVD)->is_connected())
					break;
			}

			for (int i = 0; i < sink->get_n_rvd_ports(); i++)
			{
				sink_rvd = sink->get_rvd_port(i);
				if (!sink_rvd->get_endpoint_sysface(NET_RVD)->is_connected())
					break;
			}

			// Connect RVD ports
			auto rvdlink = sys->connect(src_rvd, sink_rvd, NET_RVD);
			rvdlink->asp_get<ALinkContainment>()->add_parent_link(link);
		}
	}

	void rvd_insert_flow_convs(System* sys)
	{
		// Insert flow-converter nodes into the RVD network to convert lp_id<->flow_id.
		// This is done wherever an RVD source/sink has a RoleBinding for LP_ID.
		
		auto rvd_links = sys->get_links(NET_RVD);
		for (auto rvd_link : rvd_links)
		{
			auto src = as_a<RVDPort*>(rvd_link->get_src());
			auto sink = as_a<RVDPort*>(rvd_link->get_sink());

			for (auto port : {src, sink})
			{
				if (!port->has_role_binding(RVDPort::ROLE_DATA, "lpid"))
					continue;

				// Create flowconv node, name it after the port's hierarchical port name, relative
				// to the System.
				bool fc_to_flow = (port == src);
				std::string fc_name = hier_path_collapse(port->get_primary_port()->
					get_hier_path(sys)) + "_fc";
				auto fc_node = new NodeFlowConv(fc_to_flow);
				fc_node->set_name(fc_name);
				sys->add_child(fc_node);

				// Splice the node into the link between src/sink.
				// !!! make rvd_link point to the new fcnode->sink link in the case that
				// TWO fc_nodes are inserted on the original link. This relies on this for loop
				// visiting src first and sink second !!!
				rvd_link = sys->splice(rvd_link, fc_node->get_input(), fc_node->get_output());

				// Make the node set up its conversion tables, now that it's connected to stuff
				fc_node->configure();
			}
		}
	}

	void rvd_configure_split_nodes(System* sys)
	{
		// Now that the Split nodes have been connected to RVD links (that carry RS links),
		// each node can configure its internal routing table
		auto sp_nodes = sys->get_children_by_type<NodeSplit>();
		for (auto node : sp_nodes)
		{
			node->configure();
		}
	}

	void rvd_do_carriage(System* sys)
	{
		auto rs_links = sys->get_links(NET_RS);

		// Traverse every end-to-end RS link
		for (auto& rs_link : rs_links)
		{
			// Get e2e RS ports (physical ones, ignoring Linkpoints)
			auto rs_src = RSPort::get_rs_port(rs_link->get_src());
			auto rs_sink = RSPort::get_rs_port(rs_link->get_sink());

			// Extract their associated RVD ports
			auto e2e_rvd_src = rs_src->get_rvd_port();
			auto e2e_rvd_sink = rs_sink->get_rvd_port();

			// Initialize a carriage set to empty. This holds the Fields that need to be
			// carried across the next RVD link in the chain from e2e_sink to e2e_src.
			FieldSet carriage_set;

			// Start at the final sink and work backwards to the source
			auto cur_rvd_sink = e2e_rvd_sink;

			// Loop until we've traversed the entire chain of RVD links from sink to source
			while(true)
			{
				// First, get the local RVD link feeding the current RVD sink
				auto rvd_link_ext = cur_rvd_sink->get_endpoint_sysface(NET_RVD)->get_link0();
				assert(rvd_link_ext);

				// The next-hop feeder of the RVD link
				auto cur_rvd_src = (RVDPort*)rvd_link_ext->get_src();

				// Exit if we've reached the beginning
				if (cur_rvd_src == e2e_rvd_src)
					break;

				// Protocols of local src/sink
				auto& sink_proto = cur_rvd_sink->get_proto();
				auto& src_proto = cur_rvd_src->get_proto();

				// Carriage set += (what sink needs) - (what src provides)
				carriage_set.add(sink_proto.terminal_fields());
				carriage_set.subtract(src_proto.terminal_fields());

				// Make the intermediate Node carry the carriage set
				CarrierProtocol* carrier_proto = src_proto.get_carried_protocol();
				if (carrier_proto)
					carrier_proto->add_set(carriage_set);
				else
					carriage_set.clear();

				// Traverse backwards through the Node to find the next input RVD port to visit
				cur_rvd_sink = nullptr;
				auto src_int_ep = cur_rvd_src->get_endpoint(NET_RVD_INTERNAL, LinkFace::INNER);
				for (auto int_link : src_int_ep->links())
				{
					auto candidate = (RVDPort*)int_link->get_src();
					auto cand_rvd_link = candidate->get_endpoint_sysface(NET_RVD)->get_link0();
					auto ac = cand_rvd_link->asp_get<ALinkContainment>();
					auto cand_rs_links = ac->get_all_parent_links(NET_RS);
					for (auto cand_rs_link : cand_rs_links)
					{
						if (cand_rs_link == rs_link)
						{
							cur_rvd_sink = candidate;
							break;
						}
					}
				}
				assert(cur_rvd_sink);
			}
		}

		// Tell everyone in the System to update themselves to reflect the carriage
		sys->do_post_carriage();
	}

	void rvd_connect_clocks(System* sys)
	{
		using namespace graphs;

		// The system contains interconnect-related nodes which have no clocks connected yet.
		// When there are multiple clock domains, the choice of which clock domain to assign
		// which interconnect node is an optimization problem: we want to minimize the total number
		// of data bits that need clock crossing. This is an instance of a Multiway Graph Cut problem, 
		// which is optimally solvable in polynomial time for k=2 clocks, but is NP-hard for 3 or more.
		// We will use a greedy heuristic.

		// The implementation of the algorithm sits in a different file, and works on a generic graph.
		// Here, we need to generate this graph so that we can properly express the problem at hand.
		// In the generated graph, each vertex will correspond to a ClockResetPort of some Node, which 
		// either already has a clock assignment (making it a 'terminal' in the multiway problem) or
		// has yet to have a clock assigned, which is ultimately what we want to find out.

		// So, a Vertex represents a clock input port. An edge will represent a Conn between two DataPorts, 
		// and the two vertices of the edge are the clock input ports associated with the DataPorts. Each edge
		// will be weighted with the total number of PhysField bits in the protocol. In the case that the protocols
		// are different between the two DataPorts, we will take the union (only the physfields present in both) since
		// the missing ones will be defaulted later.

		// Graph to do multiway algorithm on
		Graph G;

		// List of terminal vertices (one for each clock domain)
		VList T;

		// Edge weights in G
		EAttr<int> weights;

		// Maps UNCONNECTED clock sinks to vertex IDs in G
		RVAttr<ClockPort*> clocksink_to_vid;
			VAttr<ClockPort*> vid_to_clocksink;

		// Maps clock sources (aka clock domains) to vertex IDs in G, and vice versa
		RVAttr<ClockPort*> clocksrc_to_vid;
		VAttr<ClockPort*> vid_to_clocksrc;
		
		// Construct G and the inputs to MWC algorithm
		auto rvd_links = sys->get_links(NET_RVD);
		for (auto rvd_link : rvd_links)
		{
			// RVD ports
			auto rvd_a = (RVDPort*)rvd_link->get_src();
			auto rvd_b = (RVDPort*)rvd_link->get_sink();
			
			// Get the clock sinks associated with each RVD port
			auto csink_a = rvd_a->get_clock_port();
			auto csink_b = rvd_b->get_clock_port();

			// Two RVD ports in same node connected to each other, under same clock domain.
			// This creates self-loops in the graph and derps up the MWC algorithm.
			if (csink_a == csink_b)
				continue;

			// Get the clock ports driving each clock sink
			auto csrc_a = csink_a->get_clock_driver();
			auto csrc_b = csink_b->get_clock_driver();

			// Vertices representing the two clock sinks. A single vertex is created for:
			// 1) Each undriven clock sink
			// 2) All clock sinks driven by the same clock source
			VertexID v_a, v_b;

			// Handle the _a and _b identically with this loop, to avoid code duplication
			for (auto& i : { std::forward_as_tuple(v_a, csrc_a, csink_a), 
				std::forward_as_tuple(v_b, csrc_b, csink_b) })
			{
				auto& v = std::get<0>(i);
				auto csrc = std::get<1>(i);
				auto csink = std::get<2>(i);

				if (csrc)
				{
					// Case 1) Clock sink is driven. Create or get the vertex associated with
					// the DRIVER of the clock sink.
					if (clocksrc_to_vid.count(csrc))
					{
						v = clocksrc_to_vid[csrc];
					}
					else
					{
						v = G.newv();
						clocksrc_to_vid[csrc] = v;
						vid_to_clocksrc[v] = csrc;

						// Also add this new vertex to our list of Terminal vertices
						T.push_back(v);
					}
				}
				else
				{
					// Case 2) Clock sink is UNdriven. Create or get the vertex associated with
					// the sink itself.
					if (clocksink_to_vid.count(csink))
					{
						v = clocksink_to_vid[csink];
					}
					else
					{
						v = G.newv();
						clocksink_to_vid[csink] = v;
							vid_to_clocksink[v] = csink;
					}
				}				
			}

			// Avoid self-loops
			if (v_a == v_b)
				continue;

			// The weight of the edge is the width of signals crossing from a to b
			int weight = PortProtocol::calc_transmitted_width(rvd_a->get_proto(), rvd_b->get_proto());
			
			// Create and add edge with weight
			EdgeID e = G.newe(v_a, v_b);
			weights[e] = weight;
		}

		/*
		sys->write_dot("rvd_premwc", NET_RVD);
		G.dump("mwc", [&](VertexID v)
		{
			ClockPort* p = nullptr;
			if (vid_to_clocksink.count(v)) p = vid_to_clocksink[v];
			if (vid_to_clocksrc.count(v))
			{
				assert(!p);
				p = vid_to_clocksrc[v];
			}
			
			return p->get_hier_path(sys) + " (" +  std::to_string(v) + ")";
		}, [&](EdgeID e)
		{
			return std::to_string(weights[e]);
		});*/

		// Call multiway cut algorithm, get vertex->terminal mapping
		auto vid_to_terminal = graphs::multi_way_cut(G, weights, T);

		// Iterate over all unconnected clock sinks, and connect them to the clock source
		// identified by the output of the MWC algorithm.
		for (auto& i : clocksink_to_vid)
		{
			ClockPort* csink = i.first;
			VertexID v_sink = i.second;

			VertexID v_src = vid_to_terminal[v_sink];
			ClockPort* csrc = vid_to_clocksrc[v_src];

			sys->connect(csrc, csink);
		}
	}

	void rvd_insert_clockx(System* sys)
	{
		// Insert clock crossing adapters on clock domain boundaries.
		// A clock domain boundary exists on any RVD link where the source/sink clock drivers differ.
		int nodenum = 0;
		auto rvd_links = sys->get_links(NET_RVD);
		
		for (auto rvd_link : rvd_links)
		{
			// RVD source/sink ports
			auto rvd_a = (RVDPort*)rvd_link->get_src();
			auto rvd_b = (RVDPort*)rvd_link->get_sink();

			// Clock sinks of each RVD port
			auto csink_a = rvd_a->get_clock_port();
			auto csink_b = rvd_b->get_clock_port();

			// Clock drivers of clock sinks of each RVD port
			auto csrc_a = csink_a->get_clock_driver();
			auto csrc_b = csink_b->get_clock_driver();

			assert(csrc_a && csrc_b);

			// We only care about mismatched domains
			if (csrc_a == csrc_b)
				continue;

			// Check to see if src has backpressure
			bool has_bp = rvd_a->has_role_binding(RVDPort::ROLE_READY);

			// Create and add clockx node
			auto cxnode = new NodeClockX(has_bp);
			cxnode->set_name("clockx" + std::to_string(nodenum++));
			sys->add_child(cxnode);

			// Connect its clock inputs
			sys->connect(csrc_a, cxnode->get_inclock_port());
			sys->connect(csrc_b, cxnode->get_outclock_port());

			// Splice the node into the existing rvd connection
			sys->splice(rvd_link, cxnode->get_indata_port(), cxnode->get_outdata_port());
		}
	}

	void rvd_connect_resets(System* sys)
	{
		// Finds danling reset inputs and connects them to either:
		// 1) Any existing reset input in the system
		// 2) A freshly-created reset input, if 1) didn't find one

		// Find or create the reset source.
		ResetPort* reset_src = nullptr;
		auto reset_srces = sys->get_children<ResetPort>([](const HierObject* o)
		{
			auto oo = as_a<const ResetPort*>(o);
			return oo && oo->get_dir() == Dir::IN;
		});

		if (!reset_srces.empty())
		{
			// Found an existing one. Any will do right now!
			reset_src = reset_srces.front();
		}
		else
		{
			// Create one.
			reset_src = new ResetPort(Dir::IN, "reset");
			reset_src->add_role_binding(ResetPort::ROLE_RESET, 
				new vlog::VlogStaticBinding("reset"));
			sys->add_port(reset_src);

			// Don't forget the actual HDL port too
			auto vinfo = (vlog::NodeVlogInfo*)sys->get_hdl_info();
			assert(vinfo);
			vinfo->add_port(new vlog::Port("reset", 1, vlog::Port::IN));
		}

		// Find any unbound reset sinks on any nodes
		auto nodes = sys->get_nodes();
		for (auto node : nodes)
		{
			auto reset_sinks = node->get_children_by_type<ResetPort>();
			for (auto reset_sink : reset_sinks)
			{
				// Sinks only
				if (reset_sink->get_dir() != Dir::IN)
					continue;

				// Unconnected sinks only.
				if (reset_sink->get_endpoint_sysface(NET_RESET)->is_connected())
					continue;

				// Connect
				sys->connect(reset_src, reset_sink);
			}
		}
	}

	// Do all work for one System
	void flow_do_system(System* sys)
	{
		// Process the RS links fresh after user's specification
		// Identify connected communication domains and assign a domain
		// index to each (physical, sans-linkpoint) RS port.
		rs_assign_domains(sys);

		// Create TOPO network from RS network
		rs_refine_to_topo(sys);

		// Assign Flow IDs to RS links and RS Ports based on topology and RS domain membership.
		// Must be done after TOPO network has been created.
		rs_assign_flow_ids(sys);

		// Create RVD network from TOPO network
		topo_refine_to_rvd(sys);
		
		// Various RVD processing
		rvd_insert_flow_convs(sys);
		rvd_configure_split_nodes(sys);
		rvd_do_carriage(sys);
		rvd_connect_clocks(sys);
		rvd_insert_clockx(sys);
		rvd_do_carriage(sys);
		rvd_connect_resets(sys);

		// Hand off to Verilog processing and output
		vlog::flow_process_system(sys);
	}
}


// Main processing entry point!
void flow_main()
{
	auto systems = genie::get_root()->get_systems();

	for (auto sys : systems)
	{
		flow_do_system(sys);

		if (Globals::inst()->dump_dot)
		{
			auto network = Network::get(Globals::inst()->dump_dot_network);
			if (!network)
				throw Exception("Unknown network " + Globals::inst()->dump_dot_network);

			sys->write_dot(sys->get_name() + "." + network->get_name(), network->get_id());
		}
	}
}

