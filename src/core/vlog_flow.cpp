#include "genie/vlog.h"
#include "genie/vlog_bind.h"

#include "genie/net_clock.h"
#include "genie/net_reset.h"
#include "genie/net_conduit.h"

using namespace genie;
using namespace genie::vlog;

namespace
{
	SystemModule* get_sysmod(System* sys)
	{
		return static_cast<SystemModule*>(sys->asp_get<AVlogInfo>()->get_module());
	}

	void flow_init_instances(System* sys)
	{
		auto sysmod = get_sysmod(sys);

		// Iterate over all Nodes and create an Instance
		for (auto& node : sys->get_nodes())
		{
			auto av = node->asp_get<AVlogInfo>();
			assert(av->get_instance() == nullptr);
			assert(av->get_module() != nullptr);

			// Create Instance
			auto inst = new Instance(node->get_name(), av->get_module());

			// Connect Instance to Node
			inst->set_node(node);

			// Connect Node to Instance
			av->set_instance(inst);

			// Add Instance fo vlog SystemModule
			sysmod->add_instance(inst);
		}
	}

	void flow_connect_rb(System* sys, 
		RoleBinding* src_rb, int src_lo, RoleBinding* sink_rb, int sink_lo, int width)
	{
		auto sysmod = get_sysmod(sys);

		// Find or create a Verilog net to connect src to sink. Depends on whether src or sink
		// belong to a top-level port (export) or not:
		//
		// Instance -> Instance : create/use net named after src port
		// Instance -> Export or
		// Export -> Instance : create/use net belonging to the export
		// Export -> Export : unsupported for now

		Net* net = nullptr;

		genie::Port* src_port = src_rb->get_parent();
		genie::Port* sink_port = sink_rb->get_parent();
		Node* src_node = src_port->get_node();
		Node* sink_node = sink_port->get_node();

		bool src_is_export = src_node == sys;
		bool sink_is_export = sink_node == sys;

		if (src_is_export && sink_is_export)
		{
			throw HierException(sys, "can not directly connect two top-level ports");
		}
		else if (sink_is_export)
		{
			// Canonicalize: if one of {src, sink} is an export, make it so that the 'src' refers
			// to the export, regardless of data flow direction
			std::swap(src_rb, sink_rb);
			std::swap(src_lo, sink_lo);
			std::swap(src_is_export, sink_is_export);
			std::swap(src_node, sink_node);
		}

		// Grab verilog-specific port bindings from signal role bindings
		auto src_b = as_a<VlogBinding*>(src_rb->get_hdl_binding());
		auto sink_b = as_a<VlogBinding*>(sink_rb->get_hdl_binding());
		assert(src_b && sink_b);

		auto src_vport = src_b->get_port();
		auto sink_vport = sink_b->get_port();
		
		// Two cases now: 
		// src_is_export=true: connecting src (an export) to sink (an instance port)
		// src_is_export=false: connecting src (an instance port) to sink (an instance port)
		// (sink_is_export is irrelevant and is false)
		if (src_is_export)
		{
			// Grab or create an ExportNet representing the top-level port.
			// Its width will be the full width of the port.
			net = sysmod->get_net(src_vport->get_name());
			if (!net)
			{
				net = sysmod->add_net(new ExportNet(src_vport));
			}
		}
		else
		{
			// Connecting two instances: it matters that src is actually an OUT and sink is
			// actually an IN (or both are inout). Swap if needed and validate.

			if (src_vport->get_dir() == vlog::Port::IN)
			{
				std::swap(src_vport, sink_vport);
				std::swap(src_node, sink_node);
				std::swap(src_b, sink_b);
				std::swap(src_is_export, sink_is_export);
				std::swap(src_lo, sink_lo);
				std::swap(src_port, sink_port);
				std::swap(src_rb, sink_rb);
			}

			if (sink_vport->get_dir() == vlog::Port::OUT)
				throw Exception("can't use output port as a sink " + sink_rb->to_string());

			Instance* src_inst = src_node->asp_get<AVlogInfo>()->get_instance();
			assert(src_inst);

			// Grab or create a WireNet connected to the source instance port.
			// Use full width of the port.
			std::string netname = src_inst->get_name() + "_" + src_vport->get_name();
			net = sysmod->get_net(netname);
			if (!net)
			{
				// Create it
				int port_width = src_vport->get_width().get_value(src_node->get_exp_resolver());
				net = sysmod->add_net(new WireNet(netname, port_width));

				// Also bind it to src, completely
				src_inst->bind_port(src_vport->get_name(), net);
			}
		}

		// We have a net that is now bound to all bits of its source. What's left is to
		// bind it to the correct bits of the sink instance port.
		//
		// The net represents the entire source port width, so lo-bit offsets on the net
		// refer to lo-bit offsets on the source port.
		Instance* sink_inst = sink_node->asp_get<AVlogInfo>()->get_instance();
		assert(sink_inst);

		sink_inst->bind_port(sink_vport->get_name(), net, width, 
			sink_b->get_lo() + sink_lo,
			src_b->get_lo() + src_lo);
	}

	void flow_connect_rb(System* sys, RoleBinding* src_rb, RoleBinding* sink_rb)
	{
		// Connect the entire width starting at the LSB

		int src_width = src_rb->get_hdl_binding()->get_width();
		int sink_width = sink_rb->get_hdl_binding()->get_width();

		if (src_width != sink_width)
		{
			throw Exception(src_rb->to_string() + " and " + sink_rb->to_string() + 
				" are different widths");
		}

		flow_connect_rb(sys, src_rb, 0, sink_rb, 0, src_width);		
	}

	void flow_do_easy_links(System* sys)
	{
		auto sysmod = get_sysmod(sys);

		// Gather links
		System::Links links;
		for (auto n : {NET_CLOCK, NET_RESET, NET_CONDUIT})
		{
			auto to_add = sys->get_links(n);
			links.insert(links.end(), to_add.begin(), to_add.end());
		}

		for (auto& link : links)
		{
			auto src = as_a<genie::Port*>(link->get_src());
			auto sink = as_a<genie::Port*>(link->get_sink());
			assert(src);
			assert(sink);

			for (auto& rb1 : src->get_role_bindings())
			{
				auto rb2 = sink->get_matching_role_binding(rb1);
				if (rb2)
					flow_connect_rb(sys, rb1, rb2);
			}
		}
	}
}

void vlog::flow_process_system(System* sys)
{
	flow_init_instances(sys);
	flow_do_easy_links(sys);

	write_system(get_sysmod(sys));
}

std::string vlog::make_default_port_name(RoleBinding* b)
{
	genie::Port* port = b->get_parent();
	Node* node = port->get_node();

	// Base name: name after port
	std::string result = port->get_name();

	// Get network and role definitions
	Network* netdef = Network::get(port->get_type());
	auto roledef = netdef->get_sig_role(b->get_id());

	bool net_has_roles = netdef->get_sig_roles().size() > 1;
	bool role_has_tags = roledef.get_uses_tags();

	// Next, add role name if the network has more than one role defined.
	if (net_has_roles)
	{
		result += "_" + roledef.get_name();
	}

	// Then, add tag name if the role has tag differentiation.
	if (role_has_tags)
	{
		result += "_" + b->get_tag();
	}

	return result;
}