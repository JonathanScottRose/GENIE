#include <unordered_set>
#include <stack>
#include <iostream>
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
#include "genie/node_reg.h"
#include "genie/flow_utils.h"

using namespace genie;
using namespace genie::graphs;

namespace std
{
    template<> class std::hash<std::pair<int,int>>
    {
    public:
        size_t operator() (const std::pair<int,int>& p) const
        {
            std::hash<int> h;
            return h(p.first) ^ h(p.second);
        }
    };
}

namespace
{
	Port* net_to_graph_topo_remap(Port* o)
	{
		// Makes split, merge, and reg nodes behave as vertices in a Topo graph by
		// melding together their input/output Topo ports into one vertex rather than two
		// disconnected ones.
		auto sp_node = as_a<NodeSplit*>(o->get_parent());
		if (sp_node) return sp_node->get_topo_input();
			
		auto mg_node = as_a<NodeMerge*>(o->get_parent());
		if (mg_node) return mg_node->get_topo_input();

		auto reg_node = as_a<NodeReg*>(o->get_parent());
		if (reg_node) return reg_node->get_topo_input();

		// Not an input/output port of a split/merge node. Do not remap.
		return o;
	}

	// Assign domains to RS Ports
	void rs_assign_domains(System* sys)
	{
		// Turn RS network into a graph.
		// Pass in a remap function which effectively treats all linkpoints as their parent ports.
		flow::N2GRemapFunc remap = [] (Port* o)
		{
			return RSPort::get_rs_port(o);
		};

		// We care about port->vid mapping and link->eid mapping
		REAttr<Link*> link_to_eid;
		RVAttr<Port*> port_to_vid;
		Graph rs_g = flow::net_to_graph(sys, NET_RS, false, nullptr, &port_to_vid, nullptr, &link_to_eid, remap);

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

	void topo_do_routing(System* sys)
	{
		auto rs_links = sys->get_links(NET_RS);

		// Turn the RS network into a graph.
		// Maintain: vertexid<->port, edgeid->link mappings
		EAttr<Link*> topo_eid_to_link;
		VAttr<Port*> topo_vid_to_port;
		RVAttr<Port*> topo_port_to_vid;
		Graph topo_g = flow::net_to_graph(sys, NET_TOPO, true, &topo_vid_to_port, &topo_port_to_vid, 
			&topo_eid_to_link, nullptr, nullptr);

        //topo_g.dump("debug", [=](VertexID v) {return topo_vid_to_port.at(v)->get_hier_path();});

		for (auto& rs_link : rs_links)
		{
			// Get endpoints
			auto rs_src = RSPort::get_rs_port(rs_link->get_src());
			auto rs_sink =  RSPort::get_rs_port(rs_link->get_sink());
			auto topo_src = rs_src->get_topo_port();
			auto topo_sink = rs_sink->get_topo_port();

			// TODO: add manual routing capability, and verify manual routes here

			// Find a route
			bool result = false;
			EList route_edges;

			// Need to check if topo ports are part of graph. They might not exist because
			// no edges exist. That's an automatic no-route.
			if(topo_port_to_vid.count(topo_src) && topo_port_to_vid.count(topo_sink))
			{
				VertexID v_src = topo_port_to_vid[topo_src];
				VertexID v_sink = topo_port_to_vid[topo_sink];
				result = graphs::dijkstra(topo_g, v_src, v_sink, nullptr, nullptr, &route_edges);
			}

			if (!result)
			{
				throw Exception("No route found from " + rs_src->get_hier_path() + " to " +
					rs_sink->get_hier_path());
			}

			// Walk the edges and associate the RS link with each constituent TOPO link
			for (auto& route_edge : route_edges)
			{
				Link* route_link = topo_eid_to_link[route_edge];
				route_link->asp_get<ALinkContainment>()->add_parent_link(rs_link);
			}
		}
	}

	void topo_prune_unused(System* sys)
	{
		// Find any TOPO links that do not carry RS links and remove them
		auto topo_links = sys->get_links(NET_TOPO);
		
		// Gather everything in an array first so we can safely iterate
		List<Link*> remove_links;
		
		for (auto link : topo_links)
		{
			auto rs_parents = link->asp_get<ALinkContainment>()->get_all_parent_links(NET_RS);
			if (rs_parents.empty())
			{
				remove_links.push_back(link);				
			}
		}
		
		// Now remove the unused links
		for (auto link : remove_links)
		{
			sys->disconnect(link);
		}
		
		// Part 2: remove any split/merge/reg nodes that are superfluous (not enough
		// inputs/output TOPO Links). Every time such a node is removed, one of two things
		// can also happen:
		// 1) If the node was not a 'dead end' and ended up having a single input link and single
		// output link, then it can be replaced with a single link. Upstream/downstream are none the wiser.
		// 2) If the node was a dead end (either had zero inputs or zero outputs) then the
		// removal of the node and any remaining outputs/inputs can potentially trigger
		// the removal of any downstream/upstream nodes.
		//
		// Because of 2), we use a stack of Nodes to examine and to potentially remove.
		// If a 2) happens, any potentially affected upstream/downstream Nodes are put on the
		// stack to examine. We stop when the chain reaction stops and no more nodes are left to examine.
		std::stack<Node*> to_examine;
		for (auto i : sys->get_nodes())
			to_examine.push(i);
		
		while (!to_examine.empty())
		{
			Node* cur_node = to_examine.top();
			to_examine.pop();
			
			// Split/merge/reg nodes have different criteria for what it means to be superfluous.
			unsigned min_inputs = 0;
			unsigned min_outputs = 0;
			TopoPort* input = nullptr;
			TopoPort* output = nullptr;
			
			if (auto n = as_a<NodeSplit*>(cur_node))
			{
				min_inputs = 1;
				min_outputs = 2;
				input = n->get_topo_input();
				output = n->get_topo_output();
			}
			else if (auto n = as_a<NodeMerge*>(cur_node))
			{
				min_inputs = 2;
				min_outputs = 1;
				input = n->get_topo_input();
				output = n->get_topo_output();
			}
			else if (auto n = as_a<NodeReg*>(cur_node))
			{
				min_inputs = 1;
				min_outputs = 1;
				input = n->get_topo_input();
				output = n->get_topo_output();
			}
			else
			{
				// We don't care about other kinds of nodes
				continue;
			}
			
			// Get the upstream/downstream ports of this Node, so we can count the number of links.
			List<Port*> input_remotes;
			List<Port*> output_remotes;
			if (input) input_remotes = input->get_endpoint_sysface(NET_TOPO)->get_remote_objs();
			if (output) output_remotes = output->get_endpoint_sysface(NET_TOPO)->get_remote_objs();
			
			// Evaluate the criteria for being removable
			if (input_remotes.size() < min_inputs || output_remotes.size() < min_outputs)
			{
                // Special case: we're removing a node that had one incoming and one outgoing link.
                // The node plus these two links will be simply replaced with a new link.
                // Before we do this, we have to save the Link Containment aspect of one of the
                // existing two to-be-removed links, which contains associations with parent RS links.
                //
                // The actual new link will be created after the old ones have been removed, below.
				List<Link*> old_parents;
                bool replace_with_link = input_remotes.size() == 1 && output_remotes.size() == 1;
				if (replace_with_link)
				{
                    Link* oldlink1 = sys->get_links(input_remotes.front(), input).front();
                    auto old_cont = oldlink1->asp_get<ALinkContainment>();
                    old_parents = old_cont->get_all_parent_links(NET_RS);
				}
				
				// Disconnect any links attached to the node we're about to remove
				if (input_remotes.size() > 0)
				{
					for (Port* remote : input_remotes)
					{
						sys->disconnect(remote, input);
						
						// Examine the affected remote node later
						if (!remote->is_export())
							to_examine.push(remote->get_node());
					}
				}
				
				if (output_remotes.size() > 0)
				{
					for (Port* remote : output_remotes)
					{
						sys->disconnect(output, remote);
						if (!remote->is_export())
							to_examine.push(remote->get_node());
					}
				}
				
                // Part 2 of the procedure outlined above.
                if (replace_with_link)
                {
                    Link* newlink = sys->connect(input_remotes.front(), output_remotes.front());
                    auto new_cont = newlink->asp_get<ALinkContainment>();
                    for (auto parent : old_parents)
                        new_cont->add_parent_link(parent);
                }
				
				// Remove the node
				sys->remove_child(cur_node->get_name());
				std::cout << "Superfluous node: removing " << cur_node->get_hier_path() << std::endl;
			}
		}
	}
	
	// Assign Flow IDs to RS links
	void rs_assign_flow_ids(System* sys)
	{
		// Get TOPO network as graph. We need link->eid and HObj->vid
		EAttr<Link*> topo_eid_to_link;
		RVAttr<Port*> topo_port_to_vid;
		Graph topo_g = flow::net_to_graph(sys, NET_TOPO, true, nullptr, &topo_port_to_vid, &topo_eid_to_link, nullptr,
			nullptr);

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
			int flow_id_width = std::max(1, util::log2(flow_id.get()));
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

	void rvd_configure_merge_nodes(System* sys)
	{
		// If full merge nodes are forced with this option, just return. Merge nodes
		// start off being the complex full ones by default.
		if (flow_options().force_full_merge)
			return;

		// Make each merge node check for mutual temporal exclusivity on its RS links, so
		// it can choose a simplified implementation
		auto mg_nodes = sys->get_children_by_type<NodeMerge>();
		for (auto node : mg_nodes)
		{
            node->configure();
			node->do_exclusion_check();
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
				auto src_int_ep = cur_rvd_src->get_endpoint(NET_RVD, LinkFace::INNER);
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

            assert(csink_a && csink_b);

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

            // Eliminiate 0 weights
            weight++;
			
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

			// Create and add clockx node
			auto cxnode = new NodeClockX();
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

	void rvd_default_eops(System* sys)
	{
		// Default unconnected EOPs to 1
		auto links = sys->get_links(NET_RVD);
		for (auto link : links)
		{
			auto src = (RVDPort*)link->get_src();
			auto sink = (RVDPort*)link->get_sink();

			auto& src_proto = src->get_proto();
			auto& sink_proto = sink->get_proto();

			Field f(NodeMerge::FIELD_EOP);
			if (sink_proto.has(f) && !src_proto.has(f))
			{
				sink_proto.set_const(f, Value(1, 1));
			}
		}
	}

	void rvd_default_flowids(System* sys)
	{
		// Try to default unconnected FlowIDs.
		// Use the unique (must be unique) FlowID of the RS link(s) going through the RVD link
		auto rvd_links = sys->get_links(NET_RVD);
		for (auto rvd_link : rvd_links)
		{
			auto src = (RVDPort*)rvd_link->get_src();
			auto sink = (RVDPort*)rvd_link->get_sink();

			auto& src_proto = src->get_proto();
			auto& sink_proto = sink->get_proto();

			Field f(NodeSplit::FIELD_FLOWID);
			if (sink_proto.has(f) && !src_proto.has(f))
			{
				// Get all RS links contained by the RVD link
				auto ac = rvd_link->asp_get<ALinkContainment>();
				auto rs_links = ac->get_all_parent_links(NET_RS);
				assert(rs_links.size() > 0);

				// Use the first one's flow id, ensure it's the rest as the others'
				auto rs_link = (RSLink*)rs_links.front();
				const Value& flow_id = rs_link->get_flow_id();

				for (auto& l : rs_links)
				{
					auto rs_link = (RSLink*)l;
					if (rs_link->get_flow_id() != flow_id)
					{
						throw HierException(sink, "tried to find a default unique Flow ID but found multiple "
						"possibilities");
					}
				}

				sink_proto.set_const(f, flow_id);
			}
		}
	}

	void rvd_do_latency_queries(System* sys)
	{
		// Get list of queries attached to the System
		auto asp = sys->asp_get<ARSLatencyQueries>();
		if (!asp)
			return;

		for (const auto& query : asp->queries())
		{
			auto rs_link = query.first;
			const auto& paramname = query.second;
			
			// RVD endpoints of the RS link
			auto e2e_sink = RSPort::get_rs_port(rs_link->get_sink())->get_rvd_port();
			auto e2e_src = RSPort::get_rs_port(rs_link->get_src())->get_rvd_port();

			// Traverse RS link from start to end, one RVD link at a time, tracing through
			// internal connections inside Nodes, and adding up their latencies
			int latency = 0;
			
			for (RVDPort* cur_src = e2e_src ; ;)
			{
				// Follow external RVD link to its sink
				auto cur_sink = (RVDPort*)cur_src->get_endpoint_sysface(NET_RVD)->get_remote_obj0();

				// Quit if reached end
				if (cur_sink == e2e_sink)
					break;

				// Go inside the current node and access internal links
				auto int_ep = cur_sink->get_endpoint(NET_RVD, LinkFace::INNER);
				if (!int_ep)
					throw HierException(cur_sink, "can't process latency query: no internal endpoint");

				// Search for the internal link that continues us along the right RS Link
				RVDLink* int_link = nullptr;
				bool found = false;

				for (auto i : int_ep->links())
				{
					int_link = (RVDLink*)i;
					cur_src = (RVDPort*)int_link->get_sink();
					if (rs_link->contained_in_rvd_port(cur_src))
					{
						found = true;
						break;
					}
				}

				if (!found)
					throw HierException(cur_sink, "can't process latency query: no internal link for carrying RS link");

				// Add latency
				latency += int_link->get_latency();

				// cur_src is already set up for next loop iteration
			}

			// Got latency? Now create parameter
			sys->define_param(paramname, latency);
		}
	}

    void rvd_add_pipeline_regs(System* sys)
    {
        // Find RVD edges with nonzero latency.
        // Splice in that many reg nodes.
        // Reset latencies to zero.

        auto rvd_links = sys->get_links(NET_RVD);
        std::vector<RVDLink*> links_to_process;
        for (auto link : rvd_links)
        {
            auto rvd_link = (RVDLink*)link;
            if (rvd_link->get_latency() > 0)
                links_to_process.push_back(rvd_link);
        }

        int pipe_no = 0;
        for (auto orig_link : links_to_process)
        {
            int latency = orig_link->get_latency();

            auto cur_link = orig_link;
            for (int i = latency-1; i >= 0; i--)
            {
                auto rg = new NodeReg(true);
                rg->set_name("pipe" + std::to_string(pipe_no) + "_" + std::to_string(i));
                sys->add_child(rg);

                cur_link = (RVDLink*)sys->splice(orig_link, rg->get_input(), rg->get_output());                
            }

            orig_link->set_latency(0);
            pipe_no++;
        }
    }

    void rvd_systolic_transform(System* sys)
    {
        // Find split nodes whose fanouts have different latencies.
        // Turn this into a chain of split nodes that feed the same destinations,
        // with the links in the split node chain 'sharing' the latency differences
        // between the original destinations. This will save registers.

        // Conditions: find split nodes that have three or more outputs. Among all the outputs,
        // at least three must have different latency values.
        auto split_nodes = sys->get_children_by_type<NodeSplit>();
        for (auto orig_sp : split_nodes)
        {
            // Sort the split node's outputs into latency bins, in increasing order.
            // std::map automatically does this for us.
            std::map<int, std::vector<TopoLink*>> lat_bins;
            for (int i = 0; i < orig_sp->get_n_outputs(); i++)
            {
                RVDPort* out_port = orig_sp->get_rvd_output(i);
                RVDLink* out_link_rvd = (RVDLink*)out_port->get_endpoint_sysface(NET_RVD)->get_link0();
                TopoLink* out_link_topo = (TopoLink*)out_link_rvd->asp_get<ALinkContainment>()->get_all_parent_links(NET_TOPO).front();
                lat_bins[out_link_rvd->get_latency()].push_back(out_link_topo);
            }

            // At least 3 different bins guarantees at least 3 split node outputs with differing latencies.
            unsigned n_bins = lat_bins.size();
            if (n_bins < 3)
                continue;

            // Grab the name and of the original split node, and destroy it!
            // Also grab the incoming topo link
            TopoLink* orig_topo = (TopoLink*)orig_sp->get_topo_input()->get_endpoint_sysface(NET_TOPO)->get_link0();
            std::string orig_name = orig_sp->get_name();
            sys->delete_object(orig_name);

            // Create split node chain
            std::vector<NodeSplit*> sp_nodes;

            // Previous split node and cumulative latency up to that point from start of chain
            NodeSplit* prev_sp = nullptr;
            int prev_lat = 0;

            for (auto bin_it = lat_bins.begin(); bin_it != lat_bins.end(); ++bin_it)
            {
                int cur_lat = bin_it->first;
                auto& cur_bin = bin_it->second;

                // Are we on the last bin?
                bool last_bin = (bin_it == --lat_bins.end());

                // Not last bin means create a split node
                if (!last_bin)
                {
                    // Create and name node
                    NodeSplit* cur_sp = new NodeSplit();
                    cur_sp->set_name(orig_name + "_systol" + std::to_string(cur_lat));
                    sys->add_child(cur_sp);
                    sp_nodes.push_back(cur_sp);

                    // Connect split node's input. If this is the first one in the chain (prev_sp is null),
                    // then use the original topo link for the deleted split node. Otherwise, create a new
                    // link to the previous split node in the chain, set its latency, and route the right RS links over it.

                    if (!prev_sp)
                    {
                        // Feed with original
                        Endpoint* cur_sp_in_ep = cur_sp->get_topo_input()->get_endpoint_sysface(NET_TOPO);
                        cur_sp_in_ep->add_link(orig_topo);
                        orig_topo->set_sink(cur_sp_in_ep);
                    }
                    else
                    {
                        // Chain to previous split node
                        TopoLink* chain_link = (TopoLink*)sys->connect(prev_sp->get_topo_output(), cur_sp->get_topo_input());
                        // Internal link within currente split node
                        TopoLink* int_link = (TopoLink*)cur_sp->get_links(cur_sp->get_topo_input(), cur_sp->get_topo_output()).front();
                        
                        // Gather RS links to route (to remainder of chain, as well as to this split node's local egress)
                        std::vector<Link*> rs_to_route;
                        for (auto bin_it2 = bin_it; bin_it2 != lat_bins.end(); ++bin_it2)
                        {
                            // For each egress topo link in this bin
                            for (auto topo_in_bin : bin_it2->second)
                            {
                                // Append RS parents
                                auto cont = topo_in_bin->asp_get<ALinkContainment>();
                                auto rs = cont->get_all_parent_links(NET_RS);
                                rs_to_route.insert(rs_to_route.end(), rs.begin(), rs.end());
                            }
                        }
                        
                        chain_link->asp_get<ALinkContainment>()->add_parent_links(rs_to_route);
                        int_link->asp_get<ALinkContainment>()->add_parent_links(rs_to_route);

                        // Latency on this new link = current cumulative latency - previous cumulative latency
                        chain_link->set_latency(cur_lat - prev_lat);
                    }

                    prev_sp = cur_sp;
                    prev_lat = cur_lat;
                } // !last_bin

                // Attach existing/former split output. The last two bins will re-use the same prev_sp.
                // Readjust the latencies of each one.
                for (auto& topo_egress : cur_bin)
                {
                    // These will end up being:
                    // (first bin's latency) for first bin
                    // (last bin - secondlast bin) for the last bin
                    // 0 for the rest in between
                    topo_egress->set_latency(cur_lat - prev_lat);

                    // Connect link to split node output, manually
                    Endpoint* prev_sp_out = prev_sp->get_topo_output()->get_endpoint_sysface(NET_TOPO);
                    prev_sp_out->add_link(topo_egress);
                    topo_egress->set_src(prev_sp_out);
                }
            } // iterate through bins

            // Iterate through created split nodes to handle RVD connectivity
            
            // Refine. Now ready for RVD connections
            for (auto sp_node : sp_nodes)
            {
                sp_node->refine(NET_RVD);
            }

            // Connect first incoming RVD link
            {
                NodeSplit* first_node = sp_nodes.front();
                Endpoint* first_input = first_node->get_rvd_input()->get_endpoint_sysface(NET_RVD);
                RVDLink* first_link = (RVDLink*)orig_topo->asp_get<ALinkContainment>()->get_all_child_links(NET_RVD).front();
                first_input->add_link(first_link);
                first_link->set_sink(first_input);
            }

            // Connect RVD links
            for (auto sp_node : sp_nodes)
            {
                // Get topo output and links
                TopoPort* topo_out = sp_node->get_topo_output();
                auto& topo_links = topo_out->get_endpoint_sysface(NET_TOPO)->links();

                // Iterate through links and create/assign to RVD link and output
                int idx = 0;
                for (auto topo_link_generic : topo_links)
                {
                    TopoLink* topo_link = (TopoLink*)topo_link_generic;
                    RVDPort* rvd_out = sp_node->get_rvd_output(idx);

                    // Check for existing RVD link
                    RVDLink* rvd_link = nullptr;
                    auto cont = topo_link->asp_get<ALinkContainment>();
                    if (cont->has_child_links())
                    {
                        // Existing link. Re-use RVD link. Just hook it up to split node output.
                        rvd_link = (RVDLink*)cont->get_child_link0();
                        auto rvd_out_ep = rvd_out->get_endpoint_sysface(NET_RVD);
                        rvd_link->set_src(rvd_out_ep);
                        rvd_out_ep->add_link(rvd_link);
                    }
                    else
                    {
                        // This means this topo link is between our newly created split nodes.
                        // Need to create a new rvd link.
                        RVDPort* rvd_sink = ((TopoPort*)topo_link->get_sink())->get_rvd_port();
                        rvd_link = (RVDLink*)sys->connect(rvd_out, rvd_sink);
                        cont->add_child_link(rvd_link);
                    }

                    rvd_link->set_latency(topo_link->get_latency());
                    idx++;
                }
            }

            // Set backpressure stuff and configure()
            for (auto sp_node : sp_nodes)
            {
                sp_node->configure();
            }
        } // end orig_sp
    }

    void rvd_do_backpressure(System* sys)
    {
        auto rvd_links = sys->get_links(NET_RVD);

        RVAttr<Port*> port_to_v;
        VAttr<Port*> v_to_port;
        Graph g = flow::net_to_graph(sys, NET_RVD, true,
            &v_to_port, &port_to_v, nullptr, nullptr);

        // Populate initial visitation list with ports that are either:
        // 1) terminal (no outgoing edges)
        // 2) Have a known/forced backpressure setting
        // Then do a depth-first reverse traversal, visiting and updating all feeders that are
        // configurable.
        std::deque<VertexID> to_visit;

        for (auto v: g.iter_verts())
        {
            bool add = g.dir_neigh(v).empty();
            add |= ((RVDPort*)(v_to_port[v]))->get_bp_status().configurable == false;
            
            if (add)
                to_visit.push_back(v);
        }

        while(!to_visit.empty())
        {
            VertexID cur_v = to_visit.back();
            to_visit.pop_back();

            auto cur_rvd = (RVDPort*)v_to_port[cur_v];
            auto& cur_bp = cur_rvd->get_bp_status();

            if (cur_bp.configurable && cur_bp.status == RVDBackpressure::UNSET)
            {
                // Given the choice, we'd like to not have this.
                cur_bp.status = RVDBackpressure::DISABLED;
            }
            else if (!cur_bp.configurable)
            {
                // If it's unchangeable, make sure it's set to something
                assert(cur_bp.status != RVDBackpressure::UNSET);
            }

            // Get feeders of this port. Could be internal or external links.
            auto feeders = g.dir_neigh_r(cur_v);

            for (auto next_v : feeders)
            {
                auto next_rvd = (RVDPort*)v_to_port[next_v];
                auto& next_bp = next_rvd->get_bp_status();
                
                // Is the feeder configurable?
                if (next_bp.configurable)
                {
                    // If it's unset, updated it to match.
                    // If it's set to DISABLED and the incoming is ENABLED, update it to match too.
                    // In both cases, put the destination on the visitation stack to continue the update.
                    //
                    // Else (it's set and: matches incoming, or is ENABLED and incoming is DISABLED),
                    // do nothing.

                    if (next_bp.status == RVDBackpressure::UNSET ||
                        next_bp.status == RVDBackpressure::DISABLED &&
                        cur_bp.status == RVDBackpressure::ENABLED)
                    {
                        next_bp.status = cur_bp.status;
                        to_visit.push_back(next_v);
                    }
                }
                else
                {
                    // Not configurable? Just make sure backpressures are compatible then.
                    assert(next_bp.status != RVDBackpressure::UNSET);
                    
                    if (next_bp.status == RVDBackpressure::DISABLED &&
                        cur_bp.status == RVDBackpressure::ENABLED)
                    {
                        throw Exception("Incompatible backpressure: " + cur_rvd->get_hier_path() +
                            " provides but " + next_rvd->get_hier_path() + " does not consume");
                    }
                }
            } // foreach feeder
        }
    }

    bool topo_optimize_check_conflict(std::vector<Link*>& set1, std::vector<Link*>& set2,
        std::unordered_set<std::pair<int,int>>& out)
    {
        // Find pairwise conflict between RSLinks in set1 and set2, and update 'out'
        // Return true if new elements were added to the set
        bool added_new = false;

        // Conflict-AVOIDING conditions:
        // - they're the same RS Link
        // - they share the same source
        // - they are explicitly marked mutually-exclusive

        for (auto link1 : set1)
        {
            for (auto link2 : set2)
            {
                if (link1 == link2)
                    continue;

                auto link1_rs = (RSLink*)link1;
                auto link2_rs = (RSLink*)link2;

                auto link1_src = RSPort::get_rs_port(link1->get_src());
                auto link2_src = RSPort::get_rs_port(link2->get_src());

                if (link1_src == link2_src)
                    continue;
                
                int fid1 = link1_rs->get_flow_id();
                int fid2 = link2_rs->get_flow_id();

                if (fid1 == fid2)
                    continue;

                auto aex = link1->asp_get<ARSExclusionGroup>();
                if (aex && aex->has(link2_rs))
                    continue;

                // Welp, they must be potentially-conflicting then.
                // Add the pair to the set, in a canonical sorted order
                std::pair<int,int> entry(fid1, fid2);

                if (fid2 < fid1)
                    std::swap(entry.first, entry.second);

                auto ins_result = out.insert(entry);
                added_new |= ins_result.second;
            }
        }

        return added_new;
    }

    void topo_optimize_fix_split(System* sys, NodeSplit* sp)
    {
        // 'sp' should have two downstream nodes. Get ports
        auto sp_topo_out = sp->get_topo_output();
        auto sp_topo_out_ep = sp_topo_out->get_endpoint_sysface(NET_TOPO);
        auto sp_downstream_ports = sp_topo_out_ep->get_remote_objs();

        assert(sp_downstream_ports.size() == 2);

        // For each of the two downstream nodes
        for (auto sp_ds_port : sp_downstream_ports)
        {
            auto sp_ds_node = as_a<NodeSplit*>(sp_ds_port->get_node());

            // Check if it's a split node that we're allowed to touch
            if (!sp_ds_node)
                continue;

            // Disconnect 'sp' from this split node
            sys->disconnect(sp_topo_out, sp_ds_port);

            // Get downstream links of this split node
            auto sp_ds_node_out = sp_ds_node->get_topo_output();
            auto reroute_links = sp_ds_node_out->get_endpoint_sysface(NET_TOPO)->links();

            // Reconnect each link's source to be 'sp'
            for (auto reroute_link : reroute_links)
            {
                reroute_link->set_src(sp_topo_out_ep);
                sp_topo_out_ep->add_link(reroute_link);
            }

            // Delete downstream split node
            sys->delete_object(sp_ds_node->get_name());
        }
    }

    void topo_optimize(System* sys)
    {
        // While can still merge more merge nodes...
        while(true)
        {
            // Keep track of whether we found a legal mergemerge candidate pair,
            // the members of the best pair, and the best pair's cost.
            bool best_found = false;
            unsigned best_cost = 0;
            NodeMerge* best_node1 = nullptr;
            NodeMerge* best_node2 = nullptr;

            // Get all current merge nodes
            auto all_merges = sys->get_children_by_type<NodeMerge>();
            
            // Loop through all pairs and find the best pair to merge
            for (auto it1 = all_merges.begin(); it1 != all_merges.end(); ++it1)
            {
                NodeMerge* node1 = *it1;
                for (auto it2 = it1 + 1; it2 != all_merges.end(); ++it2)
                {
                    NodeMerge* node2 = *it2;

                    // For each pair of merge nodes...

                    // Get merge nodes' incoming topo links
                    auto node1_topos = node1->get_topo_input()->get_endpoint_sysface(NET_TOPO)->links();
                    auto node2_topos = node2->get_topo_input()->get_endpoint_sysface(NET_TOPO)->links();

                    // Get the transmissions (RS Links) on each topo link.
                    // The outer vector index corresponds to the index in nodeX_topos vectors
                    std::vector<std::vector<Link*>> node1_rslinks, node2_rslinks;

                    for (auto tlink : node1_topos)
                    {
                        auto links = tlink->asp_get<ALinkContainment>()->get_all_parent_links(NET_RS);
                        node1_rslinks.push_back(links);
                    }

                    for (auto tlink : node2_topos)
                    {
                        auto links = tlink->asp_get<ALinkContainment>()->get_all_parent_links(NET_RS);
                        node2_rslinks.push_back(links);
                    }

                    // Create a 'conflict map': says which pairs of RS Links potentially overlap in transmission time
                    std::unordered_set<std::pair<int, int>> conflict_map;

                    // Populate conflict map with conflicts between transmissions belonging to the first merge node
                    for (auto it3 = node1_rslinks.begin(); it3 != node1_rslinks.end(); ++it3)
                    {
                        auto rslinks1 = *it3;
                        for (auto it4 = it3 + 1; it4 != node1_rslinks.end(); ++it4)
                        {
                            auto rslinks2 = *it4;
                            
                            topo_optimize_check_conflict(rslinks1, rslinks2, conflict_map);
                        }
                    }

                    // and the second node
                    for (auto it3 = node2_rslinks.begin(); it3 != node2_rslinks.end(); ++it3)
                    {
                        auto rslinks1 = *it3;
                        for (auto it4 = it3 + 1; it4 != node2_rslinks.end(); ++it4)
                        {
                            auto rslinks2 = *it4;

                            topo_optimize_check_conflict(rslinks1, rslinks2, conflict_map);
                        }
                    }

                    // The baseline level of conflict has been established. Now look to see if,
                    // by merging the two merge nodes together, a new set of conflicts would arise.
                    bool new_conflict = false;
                    for (unsigned i = 0; !new_conflict && i < node1_rslinks.size(); i++)
                    {
                        auto& rslinks1 = node1_rslinks[i];

                        for (unsigned j = 0; !new_conflict && j < node2_rslinks.size(); j++)
                        {
                            auto& rslinks2 = node2_rslinks[j];

                            // rslinks1 and rslinks2 are bundles of RSlinks associated with a specific physical
                            // merge node input.
                            // If these two physical links have the same source, we don't have to test for conflicts
                            // because they are guaranteed to have none.

                            auto src1 = node1_topos[i]->get_src();
                            auto src2 = node2_topos[j]->get_src();

                            if (src1 == src2)
                                continue;

                            // Otherwise, do the conflict check. If 'true' is returned, then new conflicts
                            // were added to the map, meaning that merging these two merge nodes is not good.
                            new_conflict |= topo_optimize_check_conflict(rslinks1, rslinks2, conflict_map);
                            if (new_conflict)
                            {
                                break;
                            }
                        }
                    } // end checking conflicts between hypothetically-merged merge nodes

                    // If NO NEW CONFLICTS are introduced, then this is a legal mergemerge.
                    // Record quality as: sum of # of inputs of first and second merge nodes
                    if (!new_conflict)
                    {
                        unsigned this_cost = node1_topos.size() + node2_topos.size();
                        if (this_cost > best_cost)
                        {
                            best_found = true;
                            best_cost = this_cost;
                            best_node1 = node1;
                            best_node2 = node2;
                        }
                    }
                }
            } // end looping through all pairs of merge nodes

            // If a legal mergemerge candidate was found, focus on the two 'best' merge nodes
            // and perform the actual mergemerge operation.
            //
            // If not? Then escape from outermost while loop and return from this function
            if (!best_found)
            {
                break;
            }

            // Arbitrarily choose which of the two merge nodes will remain, as the other one will
            // be destroyed. Let's make node1 the survivor

            // Grab all the input links of the two merge nodes
            auto node1_inputs = best_node1->get_topo_input()->get_endpoint_sysface(NET_TOPO)->links();
            auto node2_inputs = best_node2->get_topo_input()->get_endpoint_sysface(NET_TOPO)->links();

            // Get the transmissions (RS Links) on each topo link (used later)
            std::vector<std::vector<Link*>> node1_rslinks, node2_rslinks;

            for (auto tlink : node1_inputs)
            {
                auto links = tlink->asp_get<ALinkContainment>()->get_all_parent_links(NET_RS);
                node1_rslinks.push_back(links);
            }

            for (auto tlink : node2_inputs)
            {
                auto links = tlink->asp_get<ALinkContainment>()->get_all_parent_links(NET_RS);
                node2_rslinks.push_back(links);
            }

            // Disconnect node2's input links and reconnect their sources to node1's input
            for (unsigned i = 0; i < node2_inputs.size(); i++)
            {
                auto input = node2_inputs[i];
                auto input_src = input->get_src();

                sys->disconnect(input);
                auto link = sys->connect(input_src, best_node1->get_topo_input());

                // Move transmissions
                link->asp_get<ALinkContainment>()->add_parent_links(node2_rslinks[i]);
            }

            // Combine outputs: create a split node. its input: output of remaining merge node
            // its outputs: original outputs of two merge nodes
            {
                
                NodeSplit* sp = new NodeSplit();
                sp->set_name(sys->make_unique_child_name("split_auto"));
                sys->add_child(sp);

                // Get merge node destination links
                auto node1_out = best_node1->get_topo_output()->get_endpoint_sysface(NET_TOPO)->get_link0();
                auto node2_out = best_node2->get_topo_output()->get_endpoint_sysface(NET_TOPO)->get_link0();

                auto node1_sink = node1_out->get_sink();
                auto node2_sink = node2_out->get_sink();

                // Disconnect the outputs of the merge nodes
                sys->disconnect(node1_out);
                sys->disconnect(node2_out);

                // Split node's input is merge node1's output
                auto mg_to_sp = sys->connect(best_node1->get_topo_output(), sp->get_topo_input());

                // Split node's outputs go to original merge node outputs
                auto sp_to_sink1 = sys->connect(sp->get_topo_output(), node1_sink);
                auto sp_to_sink2 = sys->connect(sp->get_topo_output(), node2_sink);

                // Re-route carried RS links
                for (auto rslinks : node1_rslinks)
                {
                    mg_to_sp->asp_get<ALinkContainment>()->add_parent_links(rslinks);
                    sp_to_sink1->asp_get<ALinkContainment>()->add_parent_links(rslinks);
                }

                for (auto rslinks : node2_rslinks)
                {
                    mg_to_sp->asp_get<ALinkContainment>()->add_parent_links(rslinks);
                    sp_to_sink2->asp_get<ALinkContainment>()->add_parent_links(rslinks);
                }

                // Combine downstream split nodes with the new split node, if possible
                topo_optimize_fix_split(sys, sp);
            }

            // Delete the second merge node.
            sys->delete_object(best_node2->get_name());

        } // end while there are still merge nodes to merge
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

		// Route RS links over TOPO links
		topo_do_routing(sys);

		// Prune unused TOPO links
		topo_prune_unused(sys);
		
		// Assign Flow IDs to RS links and RS Ports based on topology and RS domain membership.
		// Must be done after TOPO network has been created.
		rs_assign_flow_ids(sys);

        // Further simplify the topology after flows have been determined
        if (flow_options().no_opt_topo == false)
        {
            topo_optimize(sys);
        }

		// Create RVD network from TOPO network
		topo_refine_to_rvd(sys);
		
		// Various RVD processing
		rvd_insert_flow_convs(sys);
		rvd_configure_split_nodes(sys);
		rvd_configure_merge_nodes(sys);
		rvd_do_carriage(sys);

        flow::apply_latency_constraints(sys);
        rvd_systolic_transform(sys);
        rvd_add_pipeline_regs(sys);
        rvd_do_carriage(sys);
        
        rvd_connect_clocks(sys);
        rvd_insert_clockx(sys);
		rvd_do_carriage(sys);
		
        rvd_do_backpressure(sys);
        rvd_connect_resets(sys);
		rvd_default_eops(sys);
		rvd_default_flowids(sys);

		rvd_do_latency_queries(sys);

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

FlowOptions& flow_options()
{
	static FlowOptions opts;
	return opts;
}