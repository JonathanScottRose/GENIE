#include <unordered_set>
#include <stack>

#include "ct/ct.h"
#include "ct/spec.h"
#include "ct/p2p.h"
#include "ct/static_init.h"
#include "ct/instance_node.h"
#include "ct/merge_node.h"
#include "ct/split_node.h"
#include "ct/flow_conv_node.h"
#include "ct/export_node.h"
#include "ct/clock_cross_node.h"
#include "ct/reg_node.h"

#include "graph.h"

using namespace ct;
using namespace ct::P2P;

namespace
{
	ct::CTOpts s_opts;

	void linktarget_to_nodeport(Spec::System* sys_spec, P2P::System* p2psys, const Spec::LinkTarget& lt,
		P2P::Node** out_node, P2P::Port** out_port)
	{
		// Locate source port and source node
		const std::string& objname = lt.get_inst();
		const std::string& ifname = lt.get_iface();
		Spec::SysObject* obj = sys_spec->get_object(objname);

		bool is_export = obj->get_type() == Spec::SysObject::EXPORT;

		P2P::Node* node = is_export ? p2psys->get_export_node() : p2psys->get_node(objname);
		P2P::Port* port = is_export ? node->get_port(objname) : node->get_port(ifname);

		if (out_node) *out_node = node;
		if (out_port) *out_port = port;
	}

	P2P::System* init_netlist(Spec::System* sys_spec)
	{
		// Create P2P system
		System* sys = new System(sys_spec->get_name());
		sys->set_spec(sys_spec);

		// 1: Create export node (and with it, top-level ports)
		ExportNode* exp_node = new ExportNode(sys, sys_spec);
		sys->add_node(exp_node);

		// 2: Create instance nodes
		for (auto& i : sys_spec->objects())
		{
			auto inst = (Spec::Instance*)i.second;
			if (inst->get_type() != Spec::SysObject::INSTANCE)
				continue;

			InstanceNode* node = new InstanceNode(inst);
			sys->add_node(node);
		}

		// 3 : Process links
		// 3a: Bin links by source linktarget
		std::unordered_map<std::string, std::vector<Spec::Link*>> binned_links;
		for (auto& linkdef : sys_spec->links())
		{
			binned_links[linkdef->get_src().get_path()].push_back(linkdef);
		}

		// Incrementing, globally-assigned Flow ID
		int flow_id = 0;

		// 3b: Iterate over bins
		for (auto& i : binned_links)
		{
			auto& bin = i.second;

			// All links in the bin share a common source. Do different things depending on whether
			// the source is an export, a broadcast linkpoint, or a unicast linkpoint

			// Locate source port and source node
			const Spec::LinkTarget& src_target = bin.front()->get_src();

			Node* src_node;
			Port* src_port;

			linktarget_to_nodeport(sys_spec, sys, src_target, &src_node, &src_port);

			// Broadcast linkpoints : all links constitute one flow (we only have these right now)
			Flow* flow = new Flow();
			flow->set_id(flow_id++);
			flow->set_src(src_port, src_target);
			src_port->add_flow(flow);
			sys->add_flow(flow);

			// For each link, either create a new flow (non-broadcast) or add to one big
			// flow (broadcast) <-- only this one exists now
			for (auto& link : bin)
			{
				const Spec::LinkTarget& dest_target = link->get_dest();
				
				Node* dest_node;
				Port* dest_port;

				linktarget_to_nodeport(sys_spec, sys, dest_target, &dest_node, &dest_port);

				flow->add_sink(dest_port, dest_target);
				dest_port->add_flow(flow);

				flow->add_link(link);
			}
		} // finish iterating over bins/sources

		return sys;
	}

	void parse_topo_node(System* sys, Spec::System* sysspec, 
		const std::string& in, Node** out_node, Port** out_port, bool is_src)
	{
		// split string
		auto dotpos = in.find_first_of('.', 0);
		std::string objname = in.substr(0, dotpos);
		std::string ifname = in.substr(dotpos+1);

		// sweet baby jesus this is bad
		Spec::TopoNode* tnode = sysspec->get_topology()->get_node(in);
		if (tnode->type == "SPLIT")
		{
			*out_node = sys->get_node(objname);
			assert((*out_node)->get_type() == Node::SPLIT);
			*out_port = is_src? 
				((SplitNode*)(*out_node))->get_free_outport() :
				((SplitNode*)(*out_node))->get_inport();
		}
		else if (tnode->type == "MERGE")
		{
			*out_node = sys->get_node(objname);
			assert((*out_node)->get_type() == Node::MERGE);
			*out_port = is_src?
				((MergeNode*)(*out_node))->get_outport() :
				((MergeNode*)(*out_node))->get_free_inport();
		}
		else if (tnode->type == "EP")
		{
			auto obj = sysspec->get_object(objname);
			switch (obj->get_type())
			{
				case Spec::SysObject::EXPORT:
					*out_node = sys->get_export_node();
					*out_port = (*out_node)->get_port(objname);
					break;
				case Spec::SysObject::INSTANCE:
					*out_node = sys->get_node(objname);
					*out_port = (*out_node)->get_port(ifname);
					break;
				default:
					assert(false);
			}
		}
		else
		{
			assert(false);
		}
	}

	void create_topology(System* sys, Spec::System* sysspec)
	{
		Spec::TopoGraph* topo = sysspec->get_topology();

		// Instantiate split and merge nodes
		for (auto& i : topo->nodes())
		{
			Spec::TopoNode* toponode = i.second;

			// based on type, make split or merge node. calculate # of inputs/outputs from topnode

			if (toponode->type == "SPLIT")
			{
				auto node = new SplitNode(toponode->name, toponode->outs.size());
				sys->add_node(node);
			}
			else if (toponode->type == "MERGE")
			{
				auto node = new MergeNode(toponode->name, toponode->ins.size());
				sys->add_node(node);
			}
			else if (toponode->type == "EP")
			{
				// nothing
			}
			else
			{
				assert(false);
			}
		}

		// Process links in topo graph into Conns
		for (auto& edge : topo->edges())
		{
			// find source node/port of each edge by parsing srcname/destname
			// fixup for exports: need to delve into sysobject type

			Node* src_node;
			Port* src_port;
			Node* dest_node;
			Port* dest_port;
			parse_topo_node(sys, sysspec, edge->src, &src_node, &src_port, true);
			parse_topo_node(sys, sysspec, edge->dest, &dest_node, &dest_port, false);

			// find or create Conn from src node and make the connection from src to dest
			Conn* conn = src_port->get_conn();
			if (!conn)
			{
				conn = sys->connect_ports(src_port, dest_port);
			}
			else
			{
				conn->add_sink(dest_port);
				assert(dest_port->get_conn() == nullptr);
				dest_port->set_conn(conn);
			}

			// go through associated Links.
			std::unordered_set<Flow*> flows_to_add;
			for (auto& link : edge->links)
			{
				// for each Link, use its src/dest LinkTargets to find correct flow this link belongs to
				// do this by going through all Flows and comparing FlowTargets

				Flow* f = nullptr;
				for (auto& i : sys->flows())
				{
					Flow* flow = i.second;

					if (flow->get_src()->lt == link->get_src())
					{
						for (auto& dest : flow->sinks())
						{
							if (dest->lt == link->get_dest())
							{
								f = flow;
								break;
							}
						}
						if (f) break;
					}
				}

				// add flow to temporary set
				assert(f);
				flows_to_add.insert(f);

				// associate Links directly with the src and dest ports too
				src_port->add_link(link);
				dest_port->add_link(link);
			}

			// after looping through links, set srcport/destport/conn flows to everything in set
			for (auto& flow : flows_to_add)
			{
				if (!src_port->has_flow(flow))
					src_port->add_flow(flow);

				if (!dest_port->has_flow(flow))
					dest_port->add_flow(flow);

				// conn doesn't contain flows? okay then
			}
		}
	}

	void connect_resets(System* sys)
	{
		// give everyone who needs it a reset connection
		for (auto& i : sys->nodes())
		{
			Node* node = i.second;

			for (auto& j : node->ports())
			{
				auto port = j.second;
				if (port->get_type() != Port::RESET || port->get_dir() != Port::IN)
					continue;

				if (!port->get_conn())
				{
					// just...find one.
					// right now, the Spec post-processing auto-creates one if none present
					sys->connect_ports(sys->get_a_reset_port(), port);
				}
			}
		}
	}

	void connect_clocks(System* sys)
	{
		using namespace ct::Graphs;

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

		Graph G;
		std::unordered_map<ClockResetPort*, VertexID> clocksink_to_vertex;
		VAttr<ClockResetPort*> vertex_to_clocksink;

		std::unordered_map<ClockResetPort*, std::vector<VertexID>> clocksrc_to_vertices;
		VAttr<ClockResetPort*> vertex_to_clocksrc;

		// First, create the vertices of the graph from all the clock sinks, and also identify terminal nodes
		for (auto& i : sys->nodes())
		{
			Node* node = i.second;

			for (auto& j : node->ports())
			{
				// Filter by clock sinks
				auto port = (ClockResetPort*)j.second;
				if (port->get_type() != Port::CLOCK || port->get_dir() != Port::IN)
					continue;
				
				// Create vertex representing the clock sink
				VertexID v = G.newv();
				clocksink_to_vertex[port] = v;
				vertex_to_clocksink[v] = port;

				// If the clock sink is already driven, it's a terminal.
				// The clock source driving the sink identifies a clock domain.
				// Keep track of which sinks are on which clock domains.
				if (port->is_connected())
				{
					auto clocksrc = (ClockResetPort*)port->get_driver();
					clocksrc_to_vertices[clocksrc].push_back(v);
				}
			}
		}

		// Then, create edges and calculate their weights
		EAttr<int> weights;
		for (auto& conn : sys->conns())
		{
			// Iterate over all Data edges
			auto data_a = (DataPort*)conn->get_source();
			if (data_a->get_type() != Port::DATA)
				continue;

			auto data_b = (DataPort*)conn->get_sink(); // data conn, only one sink

			// Get the clocks driving both ends
			ClockResetPort* clock_a = data_a->get_clock();
			ClockResetPort* clock_b = data_b->get_clock();

			// Ignore data links whose associated clock port is a clock source (this happens in exports)
			if (clock_a->get_dir() == Port::OUT || clock_b->get_dir() == Port::OUT)
			{
				continue;
			}

			// Two dataports in same node connected to each other, under same clock domain.
			// This creates self-loops in the graph and derps up the MWC algorithm.
			if (clock_a == clock_b)
				continue;

			// Look up vertices corresponding to clock sinks
			VertexID v_a = clocksink_to_vertex[clock_a];
			VertexID v_b = clocksink_to_vertex[clock_b];

			assert(v_a != v_b);

			// Determine weight based on sum of widths of fields common
			// to both ends of the Conn
			int weight = 0;
			for (auto& i : data_a->get_proto().fields())
			{
				auto pf = i.second;	
				if (data_b->get_proto().has_field(i.first))
					weight += pf->width;
			}

			// Create and add edge with weight
			EdgeID e = G.newe(v_a, v_b);
			weights[e] = weight;
		}

		VList T;

		// Merge all vertices that are driven by the same clock source, such that there's only 
		// one terminal vertex per clock source. Create the terminals array.
		for (auto& i : clocksrc_to_vertices)
		{
			VList& verts_to_merge = i.second;

			// Arbitrarily choose the first one to merge all the others with. This will be the
			// terminal vertex for this clock domain
			VertexID t = verts_to_merge.front();

			// Go through remaining vertices and merge
			auto it = verts_to_merge.begin();
			const auto& it_end = verts_to_merge.end();
			
			for (++it; it != it_end; ++it)
			{
				VertexID otherv = *it;
				G.mergev(otherv, t);
			}

			// Maintain list of terminal vertices
			T.push_back(t);

			// Remember the clock domain for each terminal vertex
			vertex_to_clocksrc[t] = i.first;
		}

		// Call multiway cut algorithm, get vertex->terminal mapping
		auto vertex_to_terminal = Graphs::multi_way_cut(G, weights, T);

		// Traverse result, connect up clocks
		for (auto& v : G.verts())
		{
			VertexID t = vertex_to_terminal[v];
			ClockResetPort* clock_src = vertex_to_clocksrc[t];
			ClockResetPort* clock_sink = vertex_to_clocksink[v];

			if (clock_sink->is_connected())
			{
				assert(clock_sink->get_driver() == clock_src);
			}
			else
			{
				sys->connect_ports(clock_src, clock_sink);
			}
		}
	}

	void insert_clock_crossings(System* sys)
	{
		// Traverse data conns to find ones where the endpoints are in two different
		// clock domains, and then splice in a clock domain converter node in there.
		//
		
		int nodenum = 0;
		auto old_conns = sys->conns();

		for (Conn* conn : old_conns)
		{
			auto data_a = (DataPort*)conn->get_source();
			if (data_a->get_type() != Port::DATA)
				continue;

			auto data_b = (DataPort*)conn->get_sink();
			auto clocksink_a = data_a->get_clock();
			auto clocksink_b = data_b->get_clock();
			auto clocksrc_a = clocksink_a->get_driver();
			auto clocksrc_b = clocksink_b->get_driver();

			assert(clocksrc_a && clocksrc_b);

			// We only care about mismatched domains
			if (clocksrc_a == clocksrc_b)
				continue;

			// Create and add clockx node
			ClockCrossNode* node = new ClockCrossNode("clockx" + std::to_string(nodenum++));
			sys->add_node(node);

			// Connect its clock inputs
			sys->connect_ports(clocksrc_a, node->get_inclock_port());
			sys->connect_ports(clocksrc_b, node->get_outclock_port());

			// Splice the node into the existing data connection
			sys->splice_conn(conn, node->get_inport(), node->get_outport());
		}
	}

	void configure_pre_negotiate(System* sys)
	{
		for (auto& i : sys->nodes())
		{
			i.second->configure_1();
		}
	}

	void insert_converters(System* sys)
	{
		// insert flow_id <-> lp_id conversion nodes

		// make a copy since we'll be modifying the actual list of connections during traversal
		auto conns = sys->conns();

		for (auto conn : conns)
		{
			Port* src = conn->get_source();
			Port* dest = conn->get_sink();

			// Special case: exporting a signal. Both ends already speak LP_ID and (hopefully) have
			// the same encoding!
			if (src->get_proto().has_field("lp_id") && dest->get_proto().has_field("lp_id"))
				continue;

			for (Port* p : {src, dest})
			{
				if (p->get_proto().has_field("lp_id"))
				{
					const std::string& nodename = p->get_parent()->get_name();
					const std::string& portname = p->get_name();
					std::string fcname = nodename + "_" + portname + "_conv";
					
					bool to_flow = (p == src);
					
					auto fc = new FlowConvNode(fcname, to_flow);

					sys->splice_conn(conn, fc->get_inport(), fc->get_outport());
					sys->add_node(fc);			
				}
			}
		}
	}

	void insert_reg_stages(System* sys)
	{
		// register outputs of merge nodes

		// make a copy since we'll be modifying the actual list of connections during traversal
		auto conns = sys->conns();

		for (auto conn : conns)
		{
			Port* src = conn->get_source();
			Port* dest = conn->get_sink();

			// only touch outputs of merge nodes
			auto mergenode = (MergeNode*)src->get_parent();
			if (mergenode->get_type() != Node::MERGE)
				continue;			

			std::string regnode_name = mergenode->get_name() + "_reg";
			RegNode* regnode = new RegNode(regnode_name);

			sys->splice_conn(conn, regnode->get_inport(), regnode->get_outport());
			sys->add_node(regnode);

			auto merge_clk_src = (ClockResetPort*)mergenode->get_clock_port()->get_first_connected_port();
			sys->connect_clock_src(regnode->get_inport(), merge_clk_src);
		}
	}

	void do_proto_carriage(System* sys)
	{
		// For each flow, work backwards from the ultimate sink and figure out which intermediate
		// ports need to carry which fields
		for (auto& i : sys->flows())
		{
			Flow* flow = i.second;
			Port* flow_src = flow->get_src()->port;

			// Skip encapsulation for clock/reset flows. 
			if (flow_src->get_type() == Port::CLOCK ||
				flow_src->get_type() == Port::RESET)
			{
				continue;
			}

			// For all sinks, work backwards to the source
			for (auto& j : flow->sinks())
			{
				Port* flow_sink = j->port;
			
				// Start with an empty carriage set at the end of the flow
				FieldSet carriage_set;
				Port* cur_sink = flow_sink;

				// Work backwards to the start of the flow
				while(true)
				{
					Port* cur_src = cur_sink->get_first_connected_port();
					if (cur_src == flow_src)
						break;

					// Add locally-required fields at sink to carriage set
					// Then, remove locally-produced fields at source form carriage set
					for (Port* p : {cur_sink, cur_src})
					{
						for (auto& j : p->get_proto().field_states())
						{
							FieldState* fs = j.second;
							Field* f = p->get_proto().get_field(j.first);
							PhysField* pf = p->get_proto().get_phys_field(fs->phys_field);

							// Ignore reverse signals and non-locals
							if (pf->sense != PhysField::FWD || !fs->is_local)
								continue;

							if (p == cur_sink)
							{
								carriage_set.add_field(*f);
							}
							else
							{
								carriage_set.remove_field(f->name);
							}
						}
					}

					// Ask the driving node to carry these fields (assumed to be co-temporal)
					Node* src_node = cur_src->get_parent();
					src_node->carry_fields(carriage_set);

					// Follow the flow backwards through the node to a new sink port
					cur_sink = src_node->rtrace(cur_src, flow);
				} // end backward pass
			}

			// Do the same thing but forwards
			FieldSet carriage_set;
			std::stack<Port*> srcs_to_visit;
			srcs_to_visit.push(flow_src);

			while (!srcs_to_visit.empty())
			{
				Port* cur_src = srcs_to_visit.top();
				srcs_to_visit.pop();

				Port* cur_sink = cur_src->get_first_connected_port();

				if (flow->has_sink(cur_sink))
					break;

				// Add locally-produced fields to the carriage set.
				// Remove locally-consumed fields.
				for (Port* p : { cur_src, cur_sink })
				{
					for (auto& j : p->get_proto().field_states())
					{
						FieldState* fs = j.second;
						Field* f = p->get_proto().get_field(j.first);
						PhysField* pf = p->get_proto().get_phys_field(fs->phys_field);

						// Ignore reverse signals and non-locals
						if (pf->sense != PhysField::FWD || !fs->is_local)
							continue;

						if (p == cur_src)
						{
							carriage_set.add_field(*f);
						}
						else
						{
							carriage_set.remove_field(f->name);
						}
					}
				}

				// Ask the sink node to carry these fields
				Node* sink_node = cur_sink->get_parent();
				sink_node->carry_fields(carriage_set);

				// Follow the flow forwards through the sink node, which could have multiple
				// exits for this flow, so add them all the the visitation stack
				auto new_srcs = sink_node->trace(cur_sink, flow);
				for (auto& k : new_srcs) srcs_to_visit.push(k);
			}
		} 
	}

	void configure_post_negotiate(System* sys)
	{
		for (auto& i : sys->nodes())
		{
			i.second->configure_2();
		}
	}

	void remove_dangling_ports(System* sys)
	{
		ExportNode* exp = sys->get_export_node();

		// Make a copy because we delete actual ports during traversal
		auto ports = exp->ports();
		for (auto& i : ports)
		{
			Port* p = i.second;
			if (p->get_conn() == nullptr)
			{
				exp->remove_port(p);
			}
		}
	}

	void handle_defaults(System* sys)
	{
		// Early version of protocol negotiation - set constant values for
		// unconnected fields.
		for (auto& i : sys->conns())
		{
			Port* src = i->get_source();
			Port* sink = i->get_sink(); // only for data connections - one sink

			for (auto& j : sink->get_proto().field_states())
			{
				FieldState* fs = j.second;
				Field* f = sink->get_proto().get_field(j.first);
				PhysField* pf = sink->get_proto().get_phys_field(fs->phys_field);

				if (pf->sense != PhysField::FWD)
					continue;

				if (src->get_proto().has_field(f->name))
					continue;

				int value;
				
				if (f->name == "valid") value = 1;
				else if (f->name == "sop") value = 1;
				else if (f->name == "eop") value = 1;
				else if (f->name == "flow_id")
				{
					auto& flows = sink->flows();
					if (flows.empty())
						continue;

					assert(flows.size() == 1);

					Flow* flow = flows.front();
					value = flow->get_id();
				}
				else
				{
					continue;
				}

				fs->is_const = true;
				fs->const_value = value;
			}

			for (auto& j : src->get_proto().field_states())
			{
				FieldState* fs = j.second;
				Field* f = src->get_proto().get_field(j.first);
				PhysField* pf = src->get_proto().get_phys_field(fs->phys_field);

				if (pf->sense != PhysField::REV)
					continue;

				if (sink->get_proto().has_field(f->name))
					continue;

				int value;

				if (f->name == "ready") value = 1;
				else
				{
					continue;
				}

				fs->is_const = true;
				fs->const_value = value;
			}
		}
	}

	void process_queries(P2P::System* p2psys, Spec::System* sys_spec)
	{
		for (const auto& query : sys_spec->latency_queries())
		{
			Spec::Link* link = sys_spec->get_link(query.link_label);
			assert(link);

			// Find the src/end Ports of this Link
			Port* cur_port;
			Port* dest_port;

			linktarget_to_nodeport(sys_spec, p2psys, link->get_src(), nullptr, &cur_port);
			linktarget_to_nodeport(sys_spec, p2psys, link->get_dest(), nullptr, &dest_port);

			// Find out which Flow this Link belongs to (ugly)
			Flow* flow = nullptr;
			for (Flow* test_flow : cur_port->flows())
			{
				for (auto test_link : test_flow->links())
				{
					if (test_link == link)
					{
						flow = test_flow;
						break;
					}
				}

				if (flow) break;
			}

			// Initialize result
			int latency = 0;

			// Traverse from link's source to sink
			while (true)
			{
				// Identify the next point-to-point hop
				Conn* conn = cur_port->get_conn();

				// Traverse the connection from its source to sink
				cur_port = conn->get_sink();

				// Reached the end? exit.
				if (cur_port == dest_port)
					break;

				// Not reached the end? Request transit through the Node
				Node* node = cur_port->get_parent();

				// If it's a reg node, add to latency
				if (node->get_type() == Node::REG_STAGE)
					latency++;

				// Search all possible outgoing ports and filter to the one containing our Link
				cur_port = nullptr;
				auto possible_ports = node->trace(cur_port, flow);
				
				for (const auto& test_port : possible_ports)
				{
					if (test_port->has_link(link))
					{
						cur_port = test_port;
						break;
					}
				}

				assert(cur_port);
			}

			// Got latency. Create parameter.
			sys_spec->set_param_binding(query.param_name, latency);
		}
	}

	std::unordered_map<std::string, std::string> s_global_params;
}

P2P::System* ct::build_system(Spec::System* system)
{
	P2P::System* result = init_netlist(system);
	create_topology(result, system);
	insert_converters(result);
	remove_dangling_ports(result);
	
	configure_pre_negotiate(result); // handles clocks on Export node, must precede clock stuff
	
	connect_clocks(result);
	insert_clock_crossings(result);

	if (s_opts.register_merge)
	{
		insert_reg_stages(result);
	}

	do_proto_carriage(result);
	configure_post_negotiate(result);
	handle_defaults(result);
	
	connect_resets(result);

	process_queries(result, system);

	return result;
}


static Util::InitShutdown s_do_init([]
{
	s_opts.register_merge = false;
});

ct::CTOpts& ct::get_opts()
{
	return s_opts;
}
	