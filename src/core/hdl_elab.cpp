#include "pch.h"
#include "hdl_elab.h"
#include "hdl.h"
#include "node_system.h"
#include "port_clockreset.h"
#include "port_conduit.h"
#include "port_rs.h"
#include "net_clockreset.h"
#include "net_conduit.h"
#include "net_rs.h"
#include "sig_role.h"

using namespace genie::impl;
using RoleBinding = genie::impl::Port::RoleBinding;
using Exception = genie::Exception;

namespace
{
	void connect_rb(NodeSystem* sys,
		Node* src_node,	const RoleBinding* src_rb, unsigned src_lsb, 
		Node* sink_node, const RoleBinding* sink_rb, unsigned sink_lsb, unsigned width)
	{
		auto& hdls = sys->get_hdl_state();

		// Get bindings and HDL ports
		auto& src_hdlb = src_rb->binding;
		auto& sink_hdlb = sink_rb->binding;

		auto src_vport = src_node->get_hdl_state().get_port(src_hdlb.get_port_name());
		auto sink_vport = sink_node->get_hdl_state().get_port(sink_hdlb.get_port_name());
		assert(src_vport && sink_vport);

		// Connect them. Take into account the RoleBinding's LSB offset into the Port as well
		// as the extra offsets passed into this function.
		hdls.connect(src_vport, sink_vport,
			src_hdlb.get_lo_slice(), (unsigned)src_hdlb.get_lo_bit() + src_lsb,
			sink_hdlb.get_lo_slice(), (unsigned)sink_hdlb.get_lo_bit() + sink_lsb, 0, width);
	}

	void tie_rb(NodeSystem* sys, Node* sink_node, RoleBinding* sink_rb, 
		const BitsVal& val, unsigned lsb)
	{
		auto& hdls = sys->get_hdl_state();

		auto& sink_hdlb = sink_rb->binding;
		auto sink_vport = sink_node->get_hdl_state().get_port(sink_hdlb.get_port_name());
		assert(sink_vport);

		hdls.connect(sink_vport, val, sink_hdlb.get_lo_slice(),
			(unsigned)sink_hdlb.get_lo_bit() + lsb);
	}
	
	void do_rs_readyvalid(NodeSystem* sys)
	{
		auto links = sys->get_links(NET_RS_PHYS);
		auto& hdls = sys->get_hdl_state();

		// For each link, try to connect valids and readies if both src/sink have them.
		// If not, try and insert defaulted signals
		for (auto link : links)
		{
			auto src = static_cast<PortRS*>(link->get_src());
			auto sink = static_cast<PortRS*>(link->get_sink());

			auto src_node = src->get_node();
			auto sink_node = sink->get_node();

			// Valid
			{
				RoleBinding* src_rb = src->get_role_binding(PortRS::VALID);
				RoleBinding* sink_rb = sink->get_role_binding(PortRS::VALID);

				if (src_rb && sink_rb)
				{
					// Both present - everything good
					hdls.connect(src_node, src_rb->binding, sink_node, sink_rb->binding);
				}
				else if (!src_rb && sink_rb)
				{
					// Connect a constant high value to the sink.
					hdls.connect(sink_node, sink_rb->binding, BitsVal(1).set_bit(0, 1));
				}
				else if (src_rb && !sink_rb)
				{
					// Bad news, this is logically wrong
					throw Exception("can't connect " + src->get_hier_path() + " to " +
						sink->get_hier_path() + " because " + src_rb->role.type.to_string() +
						" has no counterpart to connect to");
				}
				else
				{
					// Neither present? Ok, do nothing.
				}
			}

			// Ready
			{
				RoleBinding* src_rb = src->get_role_binding(PortRS::READY);
				RoleBinding* sink_rb = sink->get_role_binding(PortRS::READY);

				// Check backpressure presence
				auto src_bp = src->get_bp_status().status == RSBackpressure::ENABLED;
				auto sink_bp = sink->get_bp_status().status == RSBackpressure::ENABLED;

				// Legality check 1: if a port has backpressure, it must have a ready signal.
				if (src_bp && !src_rb)
					throw Exception(src->get_hier_path() + " has backpressure but no ready signal");
				if (sink_bp && !sink_rb)
					throw Exception(sink->get_hier_path() + " has backpressure but no ready signal");

				// Legality check 2: if sink has backpressure, src should have it too
				if (sink_bp && !src_bp)
				{
					throw Exception(src->get_hier_path() + " has no backpressure but its sink " +
						sink->get_hier_path() + " does");
				}

				// If sink has backpressure, connect ready signals together
				if (sink_bp)
				{
					hdls.connect(sink_node, sink_rb->binding, src_node, src_rb->binding);
				}
				else if (src_rb)
				{
					// Otherwise, if there's a src ready signal, tie it high
					hdls.connect(src_node, src_rb->binding, BitsVal(1).set_bit(0, 1));
				}
			}
		}
	}

	bool find_rvd_rb(PortRS* port, const FieldID& field, 
		RoleBinding** out_rb, unsigned* out_lsb)
	{
		const PortProtocol& proto = port->get_proto();
		const FieldSet& term = proto.terminal_fields();

		// Search the terminal fields
		if (term.has(field))
		{
			// Terminals always have LSB=0 relative to their rolebindings
			*out_lsb = 0;
			auto& field_bnd = proto.get_binding(field);
			*out_rb = port->get_role_binding(field_bnd);
			return true;
		}

		// Search the carrier, if exists, for the field
		const CarrierProtocol* carrier = port->get_carried_proto();
		if (carrier && carrier->has(field))
		{
			*out_rb = port->get_role_binding(PortRS::DATA_CARRIER);
			*out_lsb = carrier->get_lsb(field);
			return true;
		}

		return false;
	}

	void do_rs_fields(NodeSystem* sys)
	{
		auto phys_links = sys->get_links(NET_RS_PHYS);

		for (auto link : phys_links)
		{
			auto src = static_cast<PortRS*>(link->get_src());
			auto sink = static_cast<PortRS*>(link->get_sink());

			auto src_node = src->get_node();
			auto sink_node = sink->get_node();

			// Port protocols
			const auto& sink_proto = sink->get_proto();
			const auto& src_proto = src->get_proto();

			// Carrier protocols (if exist)
			CarrierProtocol* sink_carrier = sink->get_carried_proto();
			CarrierProtocol* src_carrier = src->get_carried_proto();

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
				RoleBinding* src_rb = src->get_role_binding(PortRS::DATA_CARRIER);
				unsigned src_lsb = src_carrier->get_domain_lsb();
				unsigned src_width = src_carrier->get_domain_width();

				RoleBinding* sink_rb = sink->get_role_binding(PortRS::DATA_CARRIER);
				unsigned sink_lsb = sink_carrier->get_domain_lsb();
				unsigned sink_width = sink_carrier->get_domain_width();

				// Make sure the widths agree
				assert(src_width == sink_width);

				// Only connect if the domain-carried width is greater than 0!
				if (src_width > 0)
				{
					connect_rb(sys, src_node, src_rb, src_lsb, sink_node, sink_rb, sink_lsb,
						src_width);
				}
			}

			// Go through all terminal and jection fields at the sink, and try connecting them
			// to either a matching field from the source port, or to a constant value.
			//
			// Include Domain fields too if we're not doing an opaque carry
			enum { TERMINAL, JECTION, DOM };
			for (auto field_type : { TERMINAL, JECTION, DOM })
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

				switch (field_type)
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
					unsigned sink_lsb;

					if (in_carry)
					{
						// Jection/domain field: rolebinding is for data carrier, may have nonzero 
						// lsb within carrier binding.
						sink_rb = sink->get_role_binding(PortRS::DATA_CARRIER);
						sink_lsb = sink_carrier->get_lsb(field.get_id());
					}
					else
					{
						// Terminal field: can lie on arbitraily-tagged rolebinding, but always
						// at lsb of 0
						const SigRoleID& role = sink_proto.get_binding(field.get_id());
						sink_rb = sink->get_role_binding(role);
						sink_lsb = 0;
					}

					// Check if the field was marked const, and if so, tie it to the const value
					if (auto const_val = sink_proto.get_const(field.get_id()))
					{
						tie_rb(sys, sink_node, sink_rb, *const_val, sink_lsb);
					}
					else
					{
						// Not const? Try and find a matching src field to make a connection with
						unsigned src_lsb;
						RoleBinding* src_rb;
						if (find_rvd_rb(src, field.get_id(), &src_rb, &src_lsb))
						{
							connect_rb(sys, src_node, src_rb, src_lsb,
								sink_node, sink_rb, sink_lsb, field.get_width());
						}
					}
				}
			}
		}
	}

	void do_non_rs(NodeSystem* sys, NetType nettype)
	{
		auto links = sys->get_links(nettype);

		for (auto link : links)
		{
			auto src = static_cast<Port*>(link->get_src());
			auto sink = static_cast<Port*>(link->get_sink());

			hdl::connect_ports(sys, src, sink);
		}
	}
}

void hdl::elab_system(NodeSystem* sys)
{
	// Resolve parameters of all child Nodes
	for (auto child : sys->get_children_by_type<Node>())
	{
		child->prepare_for_hdl();
		child->resolve_all_params();
	}

	do_non_rs(sys, NET_CLOCK);
	do_non_rs(sys, NET_RESET);
	do_non_rs(sys, NET_CONDUIT);
	
	do_rs_readyvalid(sys);
	do_rs_fields(sys);
}

void hdl::connect_ports(NodeSystem* sys, genie::impl::Port* src, genie::impl::Port* sink)
{
	auto src_node = src->get_node();
	auto sink_node = sink->get_node();

	for (auto& src_rb : src->get_role_bindings())
	{
		auto sink_rb = sink->get_role_binding(src_rb.role);
		if (!sink_rb)
			continue;

		auto type_info = genie::impl::get_sig_role(src_rb.role.type);
		assert(type_info);

		// Swap src/sink based on sense
		const RoleBinding* src_rb_t = &src_rb;
		const RoleBinding* sink_rb_t = sink_rb;
		Node* src_node_t = src_node;
		Node* sink_node_t = sink_node;
		switch (type_info->get_sense())
		{
		case SigRoleDef::REV:
		case SigRoleDef::IN:
			std::swap(src_rb_t, sink_rb_t);
			std::swap(src_node_t, sink_node_t);
		}

		// Connect entire bindings together
		connect_rb(sys,
			src_node_t, src_rb_t, 0,
			sink_node_t, sink_rb_t, 0,
			(unsigned)sink_rb_t->binding.get_bits());
	}
}
