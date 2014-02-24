#include <unordered_set>

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

	Conn* get_system_reset_conn(System* sys)
	{
		ClockResetPort* rst = sys->get_a_reset_port();
		Conn* result = rst->get_conn();

		if (result == nullptr)
		{
			result = new Conn();
			result->set_source(rst);
			rst->set_conn(result);
			sys->add_conn(result);
		}

		return result;
	}

	void connect(Port* src, Conn* sink)
	{
		assert(src->get_conn() == nullptr);
		assert(sink->get_source() == nullptr);
		src->set_conn(sink);
		sink->set_source(src);
	}

	void connect(Conn* src, Port* sink)
	{
		src->remove_sink(sink);
		src->add_sink(sink);
		assert(sink->get_conn() == nullptr);
		sink->set_conn(src);
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

			// Locate source port and source node if applicable
			const Spec::LinkTarget& src_target = bin.front()->get_src();
			const std::string& objname = src_target.get_inst();
			const std::string& ifname = src_target.get_iface();
			Spec::SysObject* src_obj = sys_spec->get_object(objname);

			bool src_is_export = src_obj->get_type() == Spec::SysObject::EXPORT;
			bool dest_is_export = sys_spec->get_object(bin.front()->get_dest().get_inst())->get_type() ==
				Spec::SysObject::EXPORT;

			Node* src_node = src_is_export ? sys->get_export_node() : sys->get_node(objname);
			Port* src_port = src_is_export ? src_node->get_port(objname) : src_node->get_port(ifname);
		
			// Clock, reset, or any kind of export - create direct connection immediately
			// and don't mess with flows
			if (src_port->get_type() == Port::CLOCK ||
				src_port->get_type() == Port::RESET || 
				src_is_export || dest_is_export)
			{
				Conn* conn = new Conn();
				conn->set_source(src_port);
				src_port->set_conn(conn);
				sys->add_conn(conn);

				for (auto& link : bin)
				{
					const Spec::LinkTarget& dest_target = link->get_dest();
					
					Node* dest_node = dest_is_export ? sys->get_export_node() :
						sys->get_node(dest_target.get_inst());
					Port* dest_port = dest_is_export ? dest_node->get_port(dest_target.get_inst()) :
						dest_node->get_port(dest_target.get_iface());

					assert(dest_port->get_type() == src_port->get_type());
					assert(dest_port->get_dir() == Port::rev_dir(src_port->get_dir()));

					conn->add_sink(dest_port);
					dest_port->set_conn(conn);
				}
			}
			else // internal data connection
			{
				Spec::Linkpoint* src_lp = sys_spec->get_linkpoint(src_target);
				bool is_broadcast = src_lp->get_type() == Spec::Linkpoint::BROADCAST;
				DataPort* src_data_port = (DataPort*)src_port;

				// Broadcast linkpoints : all links constitute one flow
				Flow* flow = nullptr;
				if (is_broadcast)
				{
					flow = new Flow();
					flow->set_id(flow_id++);
					flow->set_src(src_data_port, src_target);
					src_data_port->add_flow(flow);
					sys->add_flow(flow);
				}

				// For each link, either create a new flow (non-broadcast) or add to one big
				// flow (broadcast)
				for (auto& link : bin)
				{
					const Spec::LinkTarget& dest_target = link->get_dest();
					Spec::Linkpoint* dest_lp = sys_spec->get_linkpoint(dest_target);
					Node* dest_node = sys->get_node(dest_target.get_inst());
					DataPort* dest_data_port = (DataPort*)dest_node->get_port(dest_target.get_iface());

					if (!is_broadcast)
					{
						flow = new Flow();
						flow->set_id(flow_id++);
						flow->set_src(src_data_port, src_target);
						flow->set_sink(dest_data_port, dest_target);
						sys->add_flow(flow);

						src_data_port->add_flow(flow);
						dest_data_port->add_flow(flow);
					}
					else
					{
						flow->add_sink(dest_data_port, dest_target);
						dest_data_port->add_flow(flow);
					}
				}
			}
		} // finish iterating over bins/sources

		// 4: Fixup clocks/protocols of exported data/conduit interfaces
		for (auto& i : sys->get_export_node()->ports())
		{
			Port* exp_port = i.second;
			if (exp_port->get_type() != Port::DATA &&
				exp_port->get_type() != Port::CONDUIT)
				continue;

			Port* other_port = exp_port->get_conn()->get_sink();
			if (other_port == exp_port)
				other_port = exp_port->get_conn()->get_source();
			assert(other_port != exp_port);
			
			exp_port->set_proto(other_port->get_proto());

			// Clock fixup only for data ports
			if (exp_port->get_type() != Port::DATA)
				continue;

			DataPort* exp_data_port = (DataPort*)exp_port;
			DataPort* other_data_port = (DataPort*)other_port;

			ClockResetPort* node_clock_port = other_data_port->get_clock();
			assert(node_clock_port->get_dir() == Port::IN); // only handle this case for now

			ClockResetPort* src_clock_port = (ClockResetPort*)node_clock_port->get_conn()->get_source();

			exp_data_port->set_clock(src_clock_port);
		}

		return sys;
	}

	void create_network(System* sys)
	{
		// Collect all the inports and outports of all the instance nodes that are referened
		// by flows (ignore exported signals, internal connections only)
		std::unordered_set<DataPort*> outports_to_visit;
		std::unordered_set<DataPort*> inports_to_visit;
		std::unordered_map<DataPort*, DataPort*> inport_to_feeder;
		std::list<DataPort*> tmp;

		for (auto& it : sys->flows())
		{
			Flow* f = it.second;
			outports_to_visit.insert(f->get_src()->port);
			for (FlowTarget* sink : f->sinks())
			{
				inport_to_feeder[sink->port] = sink->port;
				inports_to_visit.insert(sink->port);
			}
		}

		// Create flow conversion nodes at outports which have a lp_id field
		tmp.assign(outports_to_visit.begin(), outports_to_visit.end());
		for (DataPort* src_port : tmp)
		{
			const Protocol& src_proto = src_port->get_proto();
			if (!src_proto.has_field("lp_id"))
				continue;

			Node* src_node = src_port->get_parent();

			FlowConvNode* fc_node = new FlowConvNode(
				sys,
				src_node->get_name() + "_" + src_port->get_name() + "_conv",
				true,
				src_proto,
				src_port->flows(),
				src_port
			);
			sys->add_node(fc_node);

			// Connect clock
			Conn* clk_conn = src_port->get_clock()->get_conn();
			connect(clk_conn, fc_node->get_clock_port());

			// Connect reset
			Conn* reset_conn = get_system_reset_conn(sys);
			connect(reset_conn, fc_node->get_reset_port());

			// Connect input
			sys->connect_ports(src_port, fc_node->get_inport());

			// Put output on to_visit list
			outports_to_visit.erase(src_port);
			outports_to_visit.insert(fc_node->get_outport());
		}

		// Do the same thing with inports
		tmp.assign(inports_to_visit.begin(), inports_to_visit.end());
		for (DataPort* dest_port : tmp)
		{
			const Protocol& dest_proto = dest_port->get_proto();
			if (!dest_proto.has_field("lp_id"))
				continue;
			
			Node* dest_node = dest_port->get_parent();

			FlowConvNode* fc_node = new FlowConvNode(
				sys,
				dest_node->get_name() + "_" + dest_port->get_name() + "_conv",
				false,
				dest_proto,
				dest_port->flows(),
				dest_port
			);
			sys->add_node(fc_node);

			// Connect clock
			Conn* clk_conn = dest_port->get_clock()->get_conn();
			connect(clk_conn, fc_node->get_clock_port());

			// Connect reset
			Conn* reset_conn = get_system_reset_conn(sys);
			connect(reset_conn, fc_node->get_reset_port());

			// Connect output
			sys->connect_ports(fc_node->get_outport(), dest_port);

			// Put input on to_visit list
			inports_to_visit.erase(dest_port);
			inports_to_visit.insert(fc_node->get_inport());
			inport_to_feeder[dest_port] = fc_node->get_inport();
		}

		// Visit each outport and construct a Split node if there is more than one unique destination
		// port referenced by all the flows associated with the outport.
		// When done, populate a map that says, for each destination port, which ports want to feed it.
		std::unordered_map<DataPort*, std::vector<DataPort*>> post_split_mapping;
		int unique_name_cnt = 0;

		for (DataPort* inst_outport : outports_to_visit)
		{
			// Enumerate all the physical destination ports referenced by all flows emanating from
			// this outport. For each one, group the flows going to that destination port.
			std::unordered_map<DataPort*, std::forward_list<Flow*>> phys_dest_map;

			for (Flow* flow : inst_outport->flows())
			{
				for (FlowTarget* phys_dest : flow->sinks())
				{
					phys_dest_map[phys_dest->port].push_front(flow);
				}
			}

			// If there's only one physical destination, no need to create a split node. Just
			// populate the thingy for the merge-creation stage.
			auto n_phys_dest = phys_dest_map.size();
			if (n_phys_dest == 1)
			{
				DataPort* phys_dest = phys_dest_map.begin()->first;
				post_split_mapping[inport_to_feeder[phys_dest]].push_back(inst_outport);
				continue; // next inst_outport
			}

			// Otherwise, create a split node and customize it
			SplitNode* split_node = new SplitNode(
				"split" + std::to_string(unique_name_cnt++),
				inst_outport->get_proto(),
				n_phys_dest
			);

			sys->add_node(split_node);

			// Connect input
			sys->connect_ports(inst_outport, split_node->get_inport());

			// Connect clock
			Conn* clk_conn = inst_outport->get_clock()->get_conn();
			connect(clk_conn, split_node->get_clock_port());

			// Connect reset
			Conn* reset_conn = get_system_reset_conn(sys);
			connect(reset_conn, split_node->get_reset_port());

			// Configure outputs and add them to the structure needed for the Split creation stage
			int outport_idx = 0;
			for (auto& i : phys_dest_map)
			{
				DataPort* phys_dest = i.first;
				DataPort* split_output = split_node->get_outport(outport_idx);

				// Update the post-split structure
				post_split_mapping[inport_to_feeder[phys_dest]].push_back(split_output);

				// Register all flows going through this output
				for (Flow* f : i.second)
				{
					split_node->register_flow(f, outport_idx);
				}
				
				outport_idx++;
			}

			// After everything registered, let the split node figure some things out about itself
			split_node->configure();
		} // next outport to visit

		// Splits are done. Now time to create Merge nodes. Visit every inport and create a Merge
		// node whenever there's more than one total physical port driving it across all incoming flows.
		unique_name_cnt = 0;

		for (DataPort* inst_inport : inports_to_visit)
		{
			auto& phys_sources = post_split_mapping[inst_inport];

			// If there's only one physical source, connect it directly to the target instance node
			// without instantiating any Split nodes
			auto n_phys_src = phys_sources.size();
			if (n_phys_src == 1)
			{
				DataPort* src = phys_sources.front();
				sys->connect_ports(src, inst_inport);
				continue; // next inst_inport
			}

			// Otherwise, create a Merge node
			MergeNode* merge_node = new MergeNode(
				"merge" + std::to_string(unique_name_cnt++),
				inst_inport->get_proto(),
				n_phys_src);

			sys->add_node(merge_node);

			// Connect output
			DataPort* merge_output = merge_node->get_outport();
			sys->connect_ports(merge_output, inst_inport);
			
			// Connect clock
			Conn* clk_conn = inst_inport->get_clock()->get_conn();
			connect(clk_conn, merge_node->get_clock_port());
			
			// Connect reset
			Conn* reset_conn = get_system_reset_conn(sys);
			connect(reset_conn, merge_node->get_reset_port());

			// Connect inports
			int inport_idx = 0;
			for (DataPort* phys_src : phys_sources)
			{
				DataPort* merge_input = merge_node->get_inport(inport_idx);

				// Connect port
				sys->connect_ports(phys_src, merge_input);

				// Register flows with merge node
				for (Flow* f : phys_src->flows())
				{
					merge_node->register_flow(f, merge_input);
				}
				
				inport_idx++;
			}
		} // next inst_inport
	}

	void post_process(System* sys)
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

		// Early version of protocol negotiation - set constant values for
		// unconnected fields.
		for (auto& i : sys->conns())
		{
			Port* src = i->get_source();
			Port* sink = i->get_sink(); // only for data connections - one sink

			for (Field* f : sink->get_proto().fields())
			{
				if (f->sense != Field::FWD)
					continue;

				if (src->get_proto().has_field(f->name))
					continue;

				int value;
				
				if (f->name == "valid") value = 1;
				else if (f->name == "sop") value = 1;
				else if (f->name == "eop") value = 1;
				else if (f->name == "flow_id")
				{
					DataPort* dsink = (DataPort*)sink;
					assert(dsink->get_type() == Port::DATA);

					auto& flows = dsink->flows();
					assert(flows.size() == 1);

					Flow* flow = flows.front();
					value = flow->get_id();
				}
				else
				{
					throw Exception("Don't know how to default field: " + f->name);
				}

				f->is_const = true;
				f->const_val = value;
			}

			for (Field* f : src->get_proto().fields())
			{
				if (f->sense != Field::REV)
					continue;

				if (sink->get_proto().has_field(f->name))
					continue;

				int value;

				if (f->name == "ready") value = 1;
				else
				{
					throw Exception("Don't know how to default field: " + f->name);
				}

				f->is_const = true;
				f->const_val = value;
			}
		}
	}
}

P2P::System* ct::build_system(Spec::System* system)
{
	P2P::System* result = init_netlist(system);
	create_network(result);
	post_process(result);
	return result;
}
