#include "pch.h"
#include "port_rs.h"
#include "port_clockreset.h"
#include "net_rs.h"
#include "net_topo.h"
#include "node_system.h"
#include "sig_role.h"
#include "genie_priv.h"

using namespace genie::impl;
using genie::HDLPortSpec;
using genie::HDLBindSpec;


namespace
{
	class PortRSInfo : public PortTypeDef
	{
	public:
		PortRSInfo()
		{
			m_short_name = "rs";
			m_default_network = NET_RS_LOGICAL;
			m_roles = { "valid", "ready", "data", "databundle", "eop", "address" };
		}

		~PortRSInfo() = default;

		Port* create_port(const std::string& name, genie::Port::Dir dir) override
		{
			return new PortRS(name, dir);
		}
	};
}

//
// Public
//

SigRoleType
	genie::PortRS::VALID,
	genie::PortRS::READY,
	genie::PortRS::DATA,
	genie::PortRS::DATABUNDLE,
	genie::PortRS::EOP,
	genie::PortRS::ADDRESS;

void PortRS::set_logic_depth(unsigned depth)
{
	m_logic_depth = depth;
}

void PortRS::set_default_packet_size(unsigned size)
{
	// Should be done for output ports only
	NodeSystem* sys = get_parent_by_type<NodeSystem>();

	// If not instantiated in a system, this is a NODE PROTOTYPE. The effective direction
	// is the nominal direction.
	
	Port::Dir eff_dir = sys ?
		get_effective_dir(sys) :
		get_dir();

	if (eff_dir != Port::Dir::OUT)
	{
		log::warn("setting default packet size on a non-source port %s has no effect",
			get_hier_path().c_str());
	}

	m_default_pkt_size = size;
}

void PortRS::set_default_importance(float imp)
{
	if (imp < 0 || imp > 1)
		throw Exception(get_hier_path() + ": importance must be between 0 and 1");

	// Should be done for output ports only
	NodeSystem* sys = get_parent_by_type<NodeSystem>();

	// If not instantiated in a system, this is a NODE PROTOTYPE. The effective direction
	// is the direction.

	Port::Dir eff_dir = sys ?
		get_effective_dir(sys) :
		get_dir();

	if (eff_dir != Port::Dir::OUT)
	{
		log::warn("setting default importance on a non-source port %s has no effect",
			get_hier_path().c_str());
	}

	m_default_importance = imp;
}

//
// Internal
//

SigRoleType genie::impl::PortRS::DATA_CARRIER;

FieldType
	genie::impl::FIELD_USERDATA,
	genie::impl::FIELD_USERADDR,
	genie::impl::FIELD_EOP,
	genie::impl::FIELD_XMIS_ID;

PortType genie::impl::PORT_RS;

void PortRS::init()
{
	VALID = register_sig_role(new SigRoleDef("valid", false));
	READY = register_sig_role(new SigRoleDef("ready", false, SigRoleDef::REV));
	DATA = register_sig_role(new SigRoleDef("data", false));
	DATABUNDLE = register_sig_role(new SigRoleDef("databundle", true));
	EOP = register_sig_role(new SigRoleDef("eop", false));
	ADDRESS = register_sig_role(new SigRoleDef("address", false));
	DATA_CARRIER = register_sig_role(new SigRoleDef("xdata", false));

	PORT_RS = register_port_type(new PortRSInfo());

	FIELD_USERDATA = register_field();
	FIELD_USERADDR = register_field();
	FIELD_XMIS_ID = register_field();
	FIELD_EOP = register_field();
}

PortRS::PortRS(const std::string & name, genie::Port::Dir dir, 
	const std::string & clk_port_name)
    : Port(name, dir), m_clk_port_name(clk_port_name),
	m_logic_depth(0),
	m_default_pkt_size(1),
	m_default_importance(1.0f),
	m_keeper_type(KeeperType::REG)
{
	make_connectable(NET_RS_LOGICAL);
	make_connectable(NET_RS_PHYS);
	make_connectable(NET_TOPO);
}

PortRS::PortRS(const std::string & name, genie::Port::Dir dir)
	: PortRS(name, dir, "")
{
}

PortRS::~PortRS()
{
}

PortRS * PortRS::clone() const
{
    return new PortRS(*this);
}

Port * PortRS::export_port(const std::string & name, NodeSystem* context)
{
	// Determine the associated clock port for the EXPORTED port.
	// This is the existing clock port that feeds the existing RS port's
	// associated clock port.
	auto old_clk_port = get_clock_port();
	if (!old_clk_port)
	{
		throw Exception(get_hier_path() + ": associated clock port " + get_clock_port_name() +
			" not found");
	}

	// Ok, now get the remote clock port feeding this one
	auto new_clk_port = old_clk_port->get_connected_clk_port(context);
	if (!new_clk_port)
	{
		throw Exception(get_hier_path() + ": can't export because associated clock port " +
			get_clock_port_name() + " is not connected yet");
	}

	// Ok, create exported port now that associated clock is known
	auto result = dynamic_cast<PortRS*>(this->clone());
	result->set_name(name);
	result->set_clock_port_name(new_clk_port->get_name());
	context->add_port(result);

	for (auto& rb : result->get_role_bindings())
	{
		auto& role = rb.role;
		auto& bind = rb.binding;
		auto old_hdl_port = get_node()->get_hdl_state().get_port(bind.get_port_name());

		// Create a new HDL port on the system
		std::string new_hdl_name;
		if (role.type == PortRS::DATABUNDLE)
			new_hdl_name = util::str_con_cat(name, "data", role.tag);
		else
			new_hdl_name = util::str_con_cat(name, util::str_tolower(role.type.to_string()));

		assert(old_hdl_port->sizes_are_resolved());
		context->get_hdl_state().get_or_create_port(new_hdl_name, old_hdl_port->get_width(),
			old_hdl_port->get_depth(), old_hdl_port->get_dir());

		// Update cloned role binding with new HDL port name.
		// It will have same width/depth but start at slice and bit 0
		bind.set_lo_bit(0);
		bind.set_lo_slice(0);
		bind.set_port_name(new_hdl_name);
	}

	return result;
}

void PortRS::reintegrate(HierObject* obj)
{
	// Copy protocol info from other port when reintegrating snapshots
	auto that = static_cast<PortRS*>(obj);

	this->m_proto = that->m_proto;
}

HierObject * PortRS::instantiate() const
{
	// Mostly like a clone, but reset ephemeral state like
	// protocol and backpressure status by not copying it

	auto result = new PortRS(get_name(), m_dir, m_clk_port_name);

	result->m_default_importance = this->m_default_importance;
	result->m_default_pkt_size = this->m_default_pkt_size;
	result->m_logic_depth = this->m_logic_depth;
	result->m_role_bindings = this->m_role_bindings;
	result->m_keeper_type = this->m_keeper_type;

	return result;
}

/*
std::vector<PortRSSub*> PortRS::get_subports() const
{
    return get_children_by_type<PortRSSub>();
}

std::vector<PortRSSub*> PortRS::get_subports(SigRoleType rtype)
{
	auto result = get_children<PortRSSub>([=](const PortRSSub* o)
	{
		return o->get_role().type == rtype;
	});

	return result;
}

PortRSSub *PortRS::get_subport(const SigRoleID& role)
{
	auto children = get_children_by_type<PortRSSub>();
	for (auto sub : children)
	{
		if (sub->get_role() == role)
			return sub;
	}

	return nullptr;
}


PortRSSub * PortRS::add_subport(const SigRoleID& role, 
	const hdl::PortBindingRef & bnd)
{
	// Check whether tag is required based on role
	if (role.type == PortRS::DATABUNDLE)
	{
		if (role.tag.empty())
		{
			throw Exception(get_hier_path() + ": role " +
				role.type.to_string() + " requires a tag");
		}
	}
	else
	{
		if (!role.tag.empty())
		{
			throw Exception(get_hier_path() + ": role " +
				role.type.to_string() + " must have empty tag");
		}
	}

	// Create subport, figure out direction and name
	auto subport_dir = get_dir();
	std::string subport_name = role.type.to_string();

	if (role.type == PortRS::READY)
	{
		subport_dir = subport_dir.rev();
	}
	else if (role.type == PortRS::DATABUNDLE)
	{
		subport_name = util::str_con_cat("data", role.tag);
	}

	util::str_makelower(subport_name);

	// Create the subport
	auto subport = new PortRSSub(subport_name, subport_dir, role);
	subport->set_hdl_binding(bnd);

	add_child(subport);
	return subport;
}
*/

PortClock * PortRS::get_clock_port() const
{
	auto obj = get_node()->get_child(get_clock_port_name());
	return dynamic_cast<PortClock*>(obj);
}

CarrierProtocol * PortRS::get_carried_proto() const
{
	// Query our node to see if it's a protocol carrier.
	auto parent = get_node();
	auto carr = dynamic_cast<ProtocolCarrier*>(parent);
	return carr ? &carr->get_carried_proto() : nullptr;
}

bool PortRS::has_field(const FieldID &f) const
{
	// check port protocol, then carried stuff, if exists
	bool result = m_proto.has_terminal_field(f);
	if (!result)
	{
		auto p = get_carried_proto();
		if (p) result = p->has(f);
	}

	return result;
}

FieldInst * PortRS::get_field(const FieldID& fid)
{
	FieldInst* result = m_proto.get_terminal_field(fid);

	if (!result)
	{
		auto p = get_carried_proto();
		if (p) result = p->get_field(fid);
	}

	return result;
}

