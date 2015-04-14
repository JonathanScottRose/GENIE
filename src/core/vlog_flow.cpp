#include "genie/vlog.h"
#include "genie/vlog_bind.h"

#include "genie/net_clock.h"
#include "genie/net_reset.h"
#include "genie/net_conduit.h"
#include "genie/net_rvd.h"

using namespace genie;
using namespace genie::vlog;

namespace
{
	SystemVlogInfo* get_sysinfo(System* sys)
	{
		return static_cast<SystemVlogInfo*>(sys->get_hdl_info());
	}

	void flow_connect_rb(System* sys, 
		RoleBinding* src_rb, int src_lsb, RoleBinding* sink_rb, int sink_lsb, int width)
	{
		auto sysinfo = get_sysinfo(sys);

		// Obtain the Verilog Port objects associated with each RoleBinding
		auto src_hdlb = as_a<VlogBinding*>(src_rb->get_hdl_binding());
		auto sink_hdlb = as_a<VlogBinding*>(sink_rb->get_hdl_binding());
		assert(src_hdlb && sink_hdlb);

		auto src_vport = src_hdlb->get_port();
		auto sink_vport = sink_hdlb->get_port();

		// Connect them. Take into account the RoleBinding's LSB offset into the Port as well
		// as the extra offsets passed into this function.
		sysinfo->connect(src_vport, sink_vport, 
			src_hdlb->get_lsb() + src_lsb,
			sink_hdlb->get_lsb() + sink_lsb,
			width);
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

	void flow_tie_rb(System* sys, RoleBinding* sink, const Value& val, int lsb)
	{
		auto sysinfo = get_sysinfo(sys);

		auto sink_hdlb = as_a<VlogBinding*>(sink->get_hdl_binding());
		assert(sink_hdlb);

		auto sink_vport = sink_hdlb->get_port();

		sysinfo->connect(sink_vport, val, lsb);
	}

	void flow_do_rvd_readyvalid(System* sys)
	{
		auto links = sys->get_links(NET_RVD);

		// For each link, try to connect valids and readies if both src/sink have them.
		// If not, try and insert defaulted signals
		for (auto link : links)
		{
			genie::Port* src = link->get_src();
			genie::Port* sink = link->get_sink();

			for (auto role : {RVDPort::ROLE_VALID, RVDPort::ROLE_READY})
			{
				RoleBinding* src_rb = src->get_role_binding(role);
				RoleBinding* sink_rb = sink->get_role_binding(role);

				// Ready travels backwards
				if (role == RVDPort::ROLE_READY)
					std::swap(src_rb, sink_rb);

				if (src_rb && sink_rb)
				{
					// Both present - everything good
					flow_connect_rb(sys, src_rb, sink_rb);
				}
				else if (!src_rb && sink_rb)
				{
					// Connect a constant high value to the sink.
					flow_tie_rb(sys, sink_rb, Value(1, 1), 0);
				}
				else if (src_rb && !sink_rb)
				{
					// Bad news, this is logically wrong
					throw Exception("can't connect " + src->get_hier_path() + " to " +
						sink->get_hier_path() + " because " + src_rb->to_string() + 
						" has no counterpart to connect to");
				}
				else
				{
					// Neither present? Ok, do nothing.
				}
			}
		}
	}

	void flow_do_easy_links(System* sys)
	{
		auto sysmod = get_sysinfo(sys);

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

			for (auto src_rb : src->get_role_bindings())
			{
				auto sink_rb = sink->get_matching_role_binding(src_rb);
				if (sink_rb)
				{
					if (src_rb->get_role_def().get_sense() == SigRole::REV)
						std::swap(src_rb, sink_rb);

					flow_connect_rb(sys, src_rb, sink_rb);
				}
			}
		}
	}
}

void vlog::flow_process_system(System* sys)
{
	flow_do_easy_links(sys);
	flow_do_rvd_readyvalid(sys);
	flow_write_system(sys);
}

HDLBinding* vlog::export_binding(System* sys, genie::Port* new_port, HDLBinding* b)
{
	auto old_b = as_a<VlogBinding*>(b);
	auto sysinfo = as_a<SystemVlogInfo*>(sys->get_hdl_info());
	
	// Create automatic name for exported signal
	auto old_rb = b->get_parent();
	const auto& roledef = old_rb->get_role_def();
	auto netdef = Network::get(new_port->get_type());

	bool net_has_roles = netdef->get_sig_roles().size() > 1;
	bool role_has_tags = roledef.get_uses_tags();

	std::string new_vportname = new_port->get_name();
	if (net_has_roles)
	{
		new_vportname += "_" + roledef.get_name();
	}
	if (role_has_tags)
	{
		new_vportname += "_" + old_rb->get_tag();
	}

	// Create new verilog port on the system. Auto name, same dir, concrete-ized width.
	auto old_vport = old_b->get_port();
	int width = old_vport->eval_width();
	auto new_vport = new Port(new_vportname, width, old_vport->get_dir());

	sysinfo->add_port(new_vport);
	
	// Create new binding to the entire verilog port
	auto result = new VlogStaticBinding(new_vportname, width, 0);
	return result;
}
