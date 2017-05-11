#include "pch.h"
#include "port_rs.h"
#include "port_clockreset.h"
#include "net_rs.h"
#include "net_topo.h"
#include "net_internal.h"
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
		}

		~PortRSInfo() = default;

		Port* create_port(const std::string& name, genie::Port::Dir dir) override
		{
			return new PortRS(name, dir);
		}
	};

	class PortRSSubInfo : public PortTypeDef
	{
	public:
		PortRSSubInfo()
		{
			m_short_name = "rs_sub";
			m_default_network = NET_RS_SUB;
		}

		~PortRSSubInfo() = default;

		Port* create_port(const std::string& name, genie::Port::Dir dir) override
		{
			// TODO: wat
			return nullptr;
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


void PortRS::add_signal(const SigRoleID& role, const std::string & sig_name,
    const std::string & width)
{
    HDLPortSpec ps;
    ps.name = sig_name;
    ps.width = width;
    ps.depth = "1";

    HDLBindSpec bs;
    bs.width = width;
    bs.depth = "1";
    bs.lo_bit = "0";
    bs.lo_slice = "0";

    add_signal_ex(role, ps, bs);
}

void PortRS::add_signal_ex(const SigRoleID& role, const HDLPortSpec& pspec, 
    const HDLBindSpec& bspec)
{
	auto node = get_node();
	if (!node)
		throw Exception(get_name() + ": port not attached to node yet");

    auto& hdl_state = node->get_hdl_state();

    // Determine required HDL port dir form role.
    // Assume it's the same as the RSPort, and invert for READY
    hdl::Port::Dir hdl_dir = hdl::Port::Dir::from_logical(get_dir());
	if (role.type == PortRS::READY)
		hdl_dir = hdl_dir.rev();

    // Get or create HDL port
    auto& hdl_port = hdl_state.get_or_create_port(pspec.name, pspec.width, pspec.depth, hdl_dir);

    hdl::PortBindingRef hdl_pb;
    hdl_pb.set_port_name(pspec.name);
    hdl_pb.set_bits(bspec.width);
    hdl_pb.set_lo_bit(bspec.lo_bit);
    hdl_pb.set_slices(bspec.depth);
    hdl_pb.set_lo_slice(bspec.lo_slice);

	add_role_binding(role, hdl_pb);
}

void PortRS::set_logic_depth(unsigned depth)
{
	m_logic_depth = depth;
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
	READY = register_sig_role(new SigRoleDef("ready", false));
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
    : Port(name, dir), m_clk_port_name(clk_port_name), m_domain_id(0),
	m_logic_depth(0)
{
	make_connectable(NET_RS_LOGICAL);
	make_connectable(NET_RS_PHYS);
	make_connectable(NET_TOPO);
	make_connectable(NET_INTERNAL, dir.rev());
}

PortRS::PortRS(const std::string & name, genie::Port::Dir dir)
	: PortRS(name, dir, "")
{
}

PortRS::PortRS(const PortRS &o)
    : Port(o), m_clk_port_name(o.m_clk_port_name), 
	m_domain_id(o.m_domain_id), m_proto(o.m_proto),
	m_logic_depth(o.m_logic_depth), m_role_bindings(o.m_role_bindings),
	m_bp_status(o.m_bp_status)
{
}

PortRS::~PortRS()
{
}

PortRS * PortRS::clone() const
{
    return new PortRS(*this);
}

void PortRS::resolve_params(ParamResolver& r)
{
	for (auto& b : m_role_bindings)
		b.binding.resolve_params(r);
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
	auto result = context->create_rs_port(name, get_dir(), new_clk_port->get_name());
	auto result_impl = dynamic_cast<PortRS*>(result);

	// Re-create subports
	/*
	for (auto old_subp : get_subports())
	{
		auto& role = old_subp->get_role();
		const auto& old_bind = old_subp->get_hdl_binding();
		const auto& old_hdl_port = get_node()->get_hdl_state().get_port(old_bind.get_port_name());

		// Create a new HDL port on the system
		std::string new_hdl_name;
		if (role.type == PortRS::DATABUNDLE)
			new_hdl_name = util::str_con_cat(name, "data", role.tag);
		else
			new_hdl_name = util::str_con_cat(name, util::str_tolower(role.type.to_string()));

		context->get_hdl_state().get_or_create_port(new_hdl_name, old_hdl_port->get_width(),
			old_hdl_port->get_depth(), old_hdl_port->get_dir());

		// Create a binding to this new HDL port.
		// It will have same width/depth but start at slice and bit 0
		auto new_bind = old_bind;
		new_bind.set_lo_bit(0);
		new_bind.set_lo_slice(0);
		new_bind.set_port_name(new_hdl_name);

		// Create the actual exported subport, and give it the new binding
		auto new_subp = new PortRSSub(old_subp->get_name(), old_subp->get_dir(), role);
		new_subp->set_hdl_binding(new_bind);

		// Add subport to exported port
		result_impl->add_child(new_subp);
	}
	*/
	for (auto& old_rb : m_role_bindings)
	{
		auto& role = old_rb.role;
		const auto& old_bind = old_rb.binding;
		const auto& old_hdl_port = get_node()->get_hdl_state().get_port(old_bind.get_port_name());

		// Create a new HDL port on the system
		std::string new_hdl_name;
		if (role.type == PortRS::DATABUNDLE)
			new_hdl_name = util::str_con_cat(name, "data", role.tag);
		else
			new_hdl_name = util::str_con_cat(name, util::str_tolower(role.type.to_string()));

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

void PortRS::reintegrate(HierObject* obj)
{
	// Copy protocol info from other port when reintegrating snapshots
	auto that = static_cast<PortRS*>(obj);

	this->m_proto = that->m_proto;
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

const std::vector<PortRS::RoleBinding>& PortRS::get_role_bindings() const
{
	return m_role_bindings;
}

std::vector<PortRS::RoleBinding> PortRS::get_role_bindings(SigRoleType rtype)
{
	std::vector<RoleBinding> result;

	std::copy_if(m_role_bindings.begin(), m_role_bindings.end(), result.end(),
		[&](const RoleBinding& rb)
	{
		return rb.role.type == rtype;
	});

	return result;
}

PortRS::RoleBinding* PortRS::get_role_binding(const SigRoleID& role)
{
	auto it = std::find_if(m_role_bindings.begin(), m_role_bindings.end(), 
		[&](const RoleBinding& rb)
	{
		return rb.role == role;
	});
	
	return (it == m_role_bindings.end()) ? nullptr : &(*it);
}


PortRS::RoleBinding& PortRS::add_role_binding(const SigRoleID& role,
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

	// Create the role binding
	if (get_role_binding(role) != nullptr)
	{
		throw Exception(get_hier_path() + ": role " + role.type.to_string() + " with tag '"
			+ role.tag + " already exists");
	}

	m_role_bindings.push_back(RoleBinding{ role, bnd });
	return m_role_bindings.back();
}

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

//
// SubPort
//

PortType genie::impl::PORT_RS_SUB;

void PortRSSub::init()
{
	PORT_RS_SUB = genie::impl::register_port_type(new PortRSSubInfo());
}

PortRSSub::PortRSSub(const std::string & name, genie::Port::Dir dir, const SigRoleID& role)
    : SubPortBase(name, dir), m_role(role)
{
	make_connectable(NET_RS_SUB);
}

PortRSSub * PortRSSub::clone() const
{
    return new PortRSSub(*this);
}

Port* PortRSSub::export_port(const std::string & name, NodeSystem* context)
{
	auto result = new PortRSSub(name, get_dir(), m_role);

	const auto& old_bnd = get_hdl_binding();

	auto& sys_hdl = context->get_hdl_state();
	auto& my_hdl = get_node()->get_hdl_state();

	auto new_bnd = old_bnd;
	// (copy width and depth)
	new_bnd.set_port_name(name);
	new_bnd.set_lo_bit(0);
	new_bnd.set_lo_slice(0);

	sys_hdl.add_port(name, new_bnd.get_bits(), new_bnd.get_slices(),
		my_hdl.get_port(old_bnd.get_port_name())->get_dir());

	return result;
}


