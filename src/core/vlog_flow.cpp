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

	void connect_rb(System* sys, 
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

	void connect_rb(System* sys, RoleBinding* src_rb, RoleBinding* sink_rb)
	{
		// Connect the entire width starting at the LSB
		int src_width = src_rb->get_hdl_binding()->get_width();
		int sink_width = sink_rb->get_hdl_binding()->get_width();

		if (src_width != sink_width)
		{
			throw Exception(src_rb->to_string() + " and " + sink_rb->to_string() + 
				" are different widths");
		}

		connect_rb(sys, src_rb, 0, sink_rb, 0, src_width);		
	}

	void tie_rb(System* sys, RoleBinding* sink, const Value& val, int lsb)
	{
		auto sysinfo = get_sysinfo(sys);

		auto sink_hdlb = as_a<VlogBinding*>(sink->get_hdl_binding());
		assert(sink_hdlb);

		auto sink_vport = sink_hdlb->get_port();

		sysinfo->connect(sink_vport, val, lsb);
	}

	void do_rvd_readyvalid(System* sys)
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
					connect_rb(sys, src_rb, sink_rb);
				}
				else if (!src_rb && sink_rb)
				{
					// Connect a constant high value to the sink.
					tie_rb(sys, sink_rb, Value(1, 1), 0);
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

	bool find_rvd_rb(RVDPort* port, const Field& field, RoleBinding** out_rb, int* out_lsb)
	{
		const PortProtocol& proto = port->get_proto();
		const FieldSet& term = proto.terminal_fields();

		// Search the terminal fields
		if (term.has(field))
		{
			// Terminals always have LSB=0 relative to their rolebindings
			*out_lsb = 0;
			const auto& rb_tag = proto.get_rvd_tag(field);
			*out_rb = port->get_role_binding(RVDPort::ROLE_DATA, rb_tag);
			return true;
		}

		// Search the carrier, if exists, for the field
		const CarrierProtocol* carrier = proto.get_carried_protocol();
		if (carrier && carrier->has(field))
		{
			*out_rb = port->get_role_binding(RVDPort::ROLE_DATA_CARRIER);
			*out_lsb = carrier->get_lsb(field);
			return true;
		}

		return false;
	}

	void do_rvd_data(System* sys)
	{
		auto rvd_links = sys->get_links(NET_RVD);

		for (auto link : rvd_links)
		{
			auto src = (RVDPort*)link->get_src();
			auto sink = (RVDPort*)link->get_sink();

			// Port protocols
			const auto& sink_proto = sink->get_proto();
			const auto& src_proto = src->get_proto();

			// Carrier protocols (if exist)
			CarrierProtocol* sink_carrier = sink_proto.get_carried_protocol();
			CarrierProtocol* src_carrier = src_proto.get_carried_protocol();

			// Special case: if both src/sink are carriers, then we will simply make a big huge
			// net connecting both of their domain regions together, rather than connecting
			// individual fields together.
			//
			// If not this case, then domain fields will be iterated over individually at the sink.
			bool opaque_domain = (src_carrier && sink_carrier);

			if (opaque_domain)
			{
				// Get the rolebindings for DATA_CARRIER at both ends and make a big fat net
				// connecting the two.
				RoleBinding* src_rb = src->get_role_binding(RVDPort::ROLE_DATA_CARRIER);
				int src_lsb = src_carrier->get_domain_lsb();
				int src_width = src_carrier->get_domain_width();

				RoleBinding* sink_rb = sink->get_role_binding(RVDPort::ROLE_DATA_CARRIER);
				int sink_lsb = sink_carrier->get_domain_lsb();
				int sink_width = sink_carrier->get_domain_width();

				// Make sure the widths agree
				assert(src_width == sink_width);
				connect_rb(sys, src_rb, src_lsb, sink_rb, sink_lsb, src_width);
			}

			// Go through all terminal and jection fields at the sink, and try connecting them
			// to either a matching field from the source port, or to a constant value.
			//
			// Include Domain fields too if we're not doing an opaque carry
			enum {TERMINAL, JECTION, DOM};
			for (auto field_type : {TERMINAL, JECTION, DOM})
			{
				// Whether or not the current field type is jection/domain, which lie inside
				// a carrier protocol (terminal fields don't)
				bool in_carry = field_type != TERMINAL;

				// Terminal/jection fieldsets don't exist without a carrier protocol.
				if (in_carry && !sink_carrier)
					continue;

				// Skip doing domain fields if we've decided to do an opaque domain carry
				if (field_type == DOM && opaque_domain)
					continue;

				// Will hold the terminal/jection/domain fieldset to iterate over
				FieldSet fields;
			
				switch(field_type)
				{
				case TERMINAL:
					fields = sink_proto.terminal_fields();
					break;
				case JECTION:
					fields = sink_carrier->jection_fields();
					break;
				case DOM:
					fields = sink_carrier->domain_fields();
					break;
				}

				// Iterate over the jection/terminal/domain fieldset
				for (const auto& field : fields.contents())
				{
					// Get rolebinding+lsb for field at sink
					RoleBinding* sink_rb;
					int sink_lsb;

					if (in_carry)
					{
						// Jection/domain field: rolebinding is for data carrier, may have nonzero 
						// lsb within carrier binding.
						sink_rb = sink->get_role_binding(RVDPort::ROLE_DATA_CARRIER);
						sink_lsb = sink_carrier->get_lsb(field);
					}
					else
					{
						// Terminal field: can lie on arbitraily-tagged DATA rolebinding, but always
						// at lsb of 0
						const auto& rb_tag = sink_proto.get_rvd_tag(field);
						sink_rb = sink->get_role_binding(RVDPort::ROLE_DATA, rb_tag);
						sink_lsb = 0;
					}

					// Check if the fieled was marked const, and if so, tie it to the const value
					if (sink_proto.is_const(field))
					{
						tie_rb(sys, sink_rb, sink_proto.get_const_value(field), sink_lsb);
					}
					else
					{
						// Not const? Try and find a matching src field to make a connection with
						int src_lsb;
						RoleBinding* src_rb;
						if (find_rvd_rb(src, field, &src_rb, &src_lsb))
						{
							connect_rb(sys, src_rb, src_lsb, sink_rb, sink_lsb, field.get_width());
						}
					}
				}
			}
		}
	}

	void do_easy_links(System* sys)
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

					connect_rb(sys, src_rb, sink_rb);
				}
			}
		}
	}
}

void vlog::flow_process_system(System* sys)
{
	do_easy_links(sys);
	do_rvd_readyvalid(sys);
	do_rvd_data(sys);
	write_system(sys);
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