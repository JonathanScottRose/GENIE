#include "pch.h"
#include "port_conduit.h"
#include "net_conduit.h"
#include "node_system.h"
#include "sig_role.h"
#include "genie_priv.h"
#include "genie/port.h"

using namespace genie::impl;

namespace
{
	class PortConduitInfo : public PortTypeDef
	{
	public:
		PortConduitInfo()
		{
			m_short_name = "conduit";
			m_default_network = NET_CONDUIT;
			m_roles = { "fwd", "rev", "in", "out", "inout" };
		}

		~PortConduitInfo() = default;

		Port* create_port(const std::string& name, genie::Port::Dir dir) override
		{
			return new PortConduit(name, dir);
		}
	};
}

//
// Public
//

SigRoleType
	genie::PortConduit::FWD,
	genie::PortConduit::REV,
	genie::PortConduit::IN,
	genie::PortConduit::OUT,
	genie::PortConduit::INOUT;

//
// Internal
//

PortType genie::impl::PORT_CONDUIT;

void PortConduit::init()
{
	PORT_CONDUIT = genie::impl::register_port_type(new PortConduitInfo());

	PortConduit::FWD = register_sig_role(new SigRoleDef( "fwd", true, SigRoleDef::FWD));
	PortConduit::REV = register_sig_role(new SigRoleDef("rev", true, SigRoleDef::REV));
	PortConduit::IN = register_sig_role(new SigRoleDef("in", true, SigRoleDef::IN));
	PortConduit::OUT = register_sig_role(new SigRoleDef("out", true, SigRoleDef::OUT));
	PortConduit::INOUT = register_sig_role(new SigRoleDef("inout", true, SigRoleDef::INOUT));
}

PortConduit::PortConduit(const std::string & name, genie::Port::Dir dir)
    : Port(name, dir)
{
	make_connectable(NET_CONDUIT);
}

PortConduit::PortConduit(const PortConduit &o)
    : Port(o)
{
}

PortConduit::~PortConduit()
{
}

PortConduit * PortConduit::clone() const
{
    return new PortConduit(*this);
}

Port * PortConduit::export_port(const std::string& name, NodeSystem* context)
{
	// TODO: this is mostly identical to PortRS::export_port except:
	// - no clock stuff
	// - HDL ports are named differently.
	// Find a way to avoid most of the code duplication?

	auto result = context->create_conduit_port(name, get_dir());
	auto result_impl = dynamic_cast<PortConduit*>(result);

	for (auto old_rb : m_role_bindings)
	{
		auto& role = old_rb.role;
		const auto& old_bind = old_rb.binding;
		auto old_hdl_port = get_node()->get_hdl_state().get_port(old_bind.get_port_name());

		// Create a new HDL port on the system
		assert(old_hdl_port->sizes_are_resolved());
		auto new_hdl_name = util::str_con_cat(name, role.tag);
		context->get_hdl_state().get_or_create_port(new_hdl_name, old_hdl_port->get_width(),
			old_hdl_port->get_depth(), old_hdl_port->get_dir());

		// Create a binding to this new HDL port.
		// It will have same width/depth but start at slice and bit 0
		auto new_bind = old_bind;
		new_bind.set_lo_bit(0);
		new_bind.set_lo_slice(0);
		new_bind.set_port_name(new_hdl_name);
		
		// Add rolebinding to exported port
		result_impl->add_role_binding(role, new_bind);
	}

	return result_impl;
}

