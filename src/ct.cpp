#include <unordered_set>
#include <stack>

#include "ct.h"
#include "spec.h"
#include "p2p.h"
#include "instance_node.h"
#include "merge_node.h"
#include "split_node.h"
#include "flow_conv_node.h"
#include "export_node.h"

using namespace ct;
using namespace ct::P2P;

namespace
{
	// Creates port for the system node, doesn't quite yet set all the properties yet like clock
	// and protocol.
	void create_top_level_port(System* sys, Spec::Export* exp_def)
	{
		Port* port;
		ExportNode* exp = sys->get_export_node();

		switch (exp_def->get_iface_type())
		{
		case Spec::Interface::CLOCK_SINK:
			port = new ClockResetPort(Port::CLOCK, Port::OUT, exp);
			break;
		case Spec::Interface::CLOCK_SRC:
			port = new ClockResetPort(Port::CLOCK, Port::IN, exp);
			break;
		case Spec::Interface::RESET_SRC:
			port = new ClockResetPort(Port::RESET, Port::IN, exp);
			break;
		case Spec::Interface::RESET_SINK:
			port = new ClockResetPort(Port::RESET, Port::OUT, exp);
			break;
		case Spec::Interface::SEND:
			port = new DataPort(exp, Port::IN);
			break;
		case Spec::Interface::RECV:
			port = new DataPort(exp, Port::OUT);
			break;
		case Spec::Interface::CONDUIT:
			port = new ConduitPort(exp);
			break;
		default:
			assert(false);
		}

		port->set_name(exp_def->get_name());
		exp->add_port(port);
	}

	P2P::System* init_netlist(Spec::System* sys_spec)
	{
		// Create P2P system
		System* sys = new System(sys_spec->get_name());
		sys->set_spec(sys_spec);

		// 1: Create instance nodes first
		for (auto& i : sys_spec->objects())
		{
			Spec::SysObject* obj = i.second;

			switch (obj->get_type())
			{
				case Spec::SysObject::INSTANCE:
				{
					InstanceNode* node = new InstanceNode((Spec::Instance*)obj);
					sys->add_node(node);
				}
				break;

				default: 
					break;
			}
		}

		// 2: Create top-level ports
		for (auto& i : sys_spec->objects())
		{
			Spec::SysObject* obj = i.second;

			switch (obj->get_type())
			{
				case Spec::SysObject::EXPORT:
					create_top_level_port(sys, (Spec::Export*)obj);
					break;

				default: break;
			}
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
			const std::string& objname = src_target.get_inst();
			const std::string& ifname = src_target.get_iface();
			Spec::SysObject* src_obj = sys_spec->get_object(objname);

			bool src_is_export = src_obj->get_type() == Spec::SysObject::EXPORT;
			bool dest_is_export = sys_spec->get_object(bin.front()->get_dest().get_inst())->get_type() ==
				Spec::SysObject::EXPORT;

			Node* src_node = src_is_export ? sys->get_export_node() : sys->get_node(objname);
			Port* src_port = src_is_export ? src_node->get_port(objname) : src_node->get_port(ifname);

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
				Node* dest_node = dest_is_export ? sys->get_export_node() : 
					sys->get_node(dest_target.get_inst());
				Port* dest_port = dest_is_export? dest_node->get_port(dest_target.get_inst()) :
					dest_node->get_port(dest_target.get_iface());

				flow->add_sink(dest_port, dest_target);
				dest_port->add_flow(flow);
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

					if (flow->get_src()->lt == link.get_src())
					{
						for (auto& dest : flow->sinks())
						{
							if (dest->lt == link.get_dest())
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

	void set_clock_on_port(System* sys, DataPort* target_port, ClockResetPort* clock_src)
	{
		auto target_clock_port = target_port->get_clock();

		// hack: if the target port references a null clock port, the target port probably
		// belongs to an export node. in this case, the clock source is likely ON the export node
		// itself, so let's set it here.
		if (!target_clock_port)
		{
			assert(clock_src->get_parent()->get_type() == Node::EXPORT && 
				clock_src->get_parent() == target_port->get_parent());

			target_port->set_clock(clock_src);
			target_clock_port = clock_src;
			return; // done here, no connections need to be made
		}

		auto existing_clock_src = target_clock_port->get_first_connected_port();
		if (existing_clock_src)
		{
			// we don't know how to handle this otherwise yet
			assert(existing_clock_src == clock_src);
		}
		else
		{
			sys->connect_ports(clock_src, target_clock_port);
		}
	}

	void fixup_clocks_resets(System* sys)
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

		// dirt-stupid clock domain matching.
		// follow all flows from their source to their sinks, dispensing the same clock assignment
		// as found at the source.
		//
		// IF the source is an export, which lacks a clock assignment, use the flow destination's
		// clock assignment instead.
		for (auto& i : sys->flows())
		{
			Flow* flow = i.second;
			auto flow_src = (DataPort*)flow->get_src()->port;
			if (flow_src->get_type() != Port::DATA)
				continue; // only data ports have clock assignments

			// Get source port's clock port
			auto flow_src_clock_port = (ClockResetPort*)flow_src->get_clock();

			// Follow the source port's clock port to its source to get the clock driver port
			ClockResetPort* clock_src = flow_src_clock_port ?
				(ClockResetPort*)flow_src_clock_port->get_first_connected_port() : nullptr;

			// Try the flow's destination if querying the source fails (happens when an export is
			// the source of a flow)
			if (!clock_src)
			{
				clock_src = (ClockResetPort*)
					((DataPort*)flow->get_sink0()->port)->get_clock()->get_first_connected_port();
			}

			assert(clock_src->get_type() == Port::CLOCK && clock_src->get_dir() == Port::OUT);

			std::stack<DataPort*> ports_to_visit;
			ports_to_visit.push(flow_src);

			while (!ports_to_visit.empty())
			{
				DataPort* src = ports_to_visit.top();
				ports_to_visit.pop();

				// Check/fixup existing assignment
				set_clock_on_port(sys, src, clock_src);

				// we assume only one fanout since these are data ports.
				// fixup its clock as well
				auto dest = (DataPort*)src->get_first_connected_port();
				set_clock_on_port(sys, dest, clock_src);

				// we just arrived at an input to some node. follow the flow THROUGH this input
				// to the node's outputs, for this flow only.
				auto new_srces = dest->get_parent()->trace(dest, flow);
				for (auto& new_src : new_srces)
				{
					ports_to_visit.push((DataPort*)new_src);
				}

				// repeat the process, depth-first
			}
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
		// modify the actual netlist AFTER traversing it, since we don't want to change
		// sys->conns while iterating through them
		std::forward_list<std::pair<Conn*, FlowConvNode*>> to_add;

		for (auto conn : sys->conns())
		{
			Port* src = conn->get_source();
			Port* dest = conn->get_sink();

			for (Port* p : {src, dest})
			{
				if (p->get_proto().has_field("lp_id"))
				{
					const std::string& nodename = p->get_parent()->get_name();
					const std::string& portname = p->get_name();
					std::string fcname = nodename + "_" + portname + "_conv";
					
					bool to_flow = (p == src);
					
					auto fc = new FlowConvNode(fcname, to_flow);
					to_add.emplace_front(conn, fc); // defer until after traversal
				}
			}
		}

		// do netlist modification now
		for (auto& i : to_add)
		{
			auto conn = i.first;
			auto fc = i.second;
			sys->splice_conn(conn, fc->get_inport(), fc->get_outport());
			sys->add_node(fc);
		}
	}

	void do_proto_carriage(System* sys)
	{
		// For each flow, work backwards from the ultimate sink and figure out which intermediate
		// ports need to carry which fields
		for (auto i : sys->flows())
		{
			Flow* flow = i.second;
			Port* flow_sink = flow->get_sink0()->port;
			Port* flow_src = flow->get_src()->port;

			// Skip encapsulation for clock/reset flows. They have multiple-fanout local connections
			// and that might break this algorithm.
			if (flow_sink->get_type() == Port::CLOCK ||
				flow_sink->get_type() == Port::RESET)
			{
				continue;
			}

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

		// Remove dangling ports from system
		std::forward_list<Port*> ports_to_delete;
		for (auto& i : exp->ports())
		{
			Port* p = i.second;
			if (p->get_conn() == nullptr)
			{
				ports_to_delete.push_front(p);
			}
		}

		for (Port* p : ports_to_delete)
		{
			exp->remove_port(p);
			delete p;
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
}

P2P::System* ct::build_system(Spec::System* system)
{
	P2P::System* result = init_netlist(system);
	create_topology(result, system);
	insert_converters(result);
	remove_dangling_ports(result);
	configure_pre_negotiate(result);
	fixup_clocks_resets(result);
	do_proto_carriage(result);
	configure_post_negotiate(result);
	handle_defaults(result);
	//result->dump_graph();
	return result;
}
