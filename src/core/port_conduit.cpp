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
		}

		~PortConduitInfo() = default;

		Port* create_port(const std::string& name, genie::Port::Dir dir) override
		{
			return new PortConduit(name, dir);
		}
	};

	class PortConduitSubInfo : public PortTypeDef
	{
	public:
		PortConduitSubInfo()
		{
			m_short_name = "conduit_sub";
			m_default_network = NET_CONDUIT_SUB;
		}

		~PortConduitSubInfo() = default;

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
	genie::PortConduit::FWD,
	genie::PortConduit::REV,
	genie::PortConduit::IN,
	genie::PortConduit::OUT,
	genie::PortConduit::INOUT;

void PortConduit::add_signal(const SigRoleID& role, 
    const std::string& sig_name, const std::string& width)
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

void PortConduit::add_signal_ex(const SigRoleID& role, 
    const genie::HDLPortSpec &pspec, const genie::HDLBindSpec &bspec)
{
    // Create new, or validate existing, HDL port
	auto node = get_node();
	if (!node)
		throw Exception(get_name() + ": port not attached to node yet");

    auto& hdl_state = node->get_hdl_state();

    hdl::Port::Dir hdl_dir = hdl::Port::Dir::INOUT;
    if (role.type == PortConduit::IN) 
		hdl_dir = hdl::Port::Dir::IN;
	else if (role.type == PortConduit::OUT) 
		hdl_dir = hdl::Port::Dir::OUT; 
	else if (role.type == PortConduit::INOUT) 
		hdl_dir = hdl::Port::Dir::INOUT; 
	else if (role.type == PortConduit::FWD) 
		hdl_dir = hdl::Port::Dir::from_logical(get_dir());
	else if (role.type == PortConduit::REV)
	{
		hdl_dir = hdl::Port::Dir::from_logical(get_dir());
		hdl_dir = hdl_dir.rev();
	}
	else
	{
        assert(false);
    }

    hdl_state.get_or_create_port(pspec.name, pspec.width, pspec.depth, hdl_dir);

	// Convert to PortBindingRef
    hdl::PortBindingRef hdl_pb;
    hdl_pb.set_port_name(pspec.name);
    hdl_pb.set_bits(bspec.width);
    hdl_pb.set_lo_bit(bspec.lo_bit);
    hdl_pb.set_slices(bspec.depth);
    hdl_pb.set_lo_slice(bspec.lo_slice);

	// Create the actual subport
	add_subport(role, hdl_pb);
}

//
// Internal
//

PortType genie::impl::PORT_CONDUIT;

void PortConduit::init()
{
	PORT_CONDUIT = genie::impl::register_port_type(new PortConduitInfo());

	genie::PortConduit::FWD = genie::impl::register_sig_role(new SigRoleDef( "fwd", true ));
	genie::PortConduit::REV = genie::impl::register_sig_role(new SigRoleDef("rev", true));
	genie::PortConduit::IN = genie::impl::register_sig_role(new SigRoleDef("in", true));
	genie::PortConduit::OUT = genie::impl::register_sig_role(new SigRoleDef("out", true));
	genie::PortConduit::INOUT = genie::impl::register_sig_role(new SigRoleDef("inout", true));
}

PortConduit::PortConduit(const std::string & name, genie::Port::Dir dir)
    : Port(name, dir)
{
	make_connectable(NET_CONDUIT);
}

PortConduit::PortConduit(const PortConduit &o)
    : Port(o)
{
    // Copy role-things?
}

PortConduit::~PortConduit()
{
}

PortConduit * PortConduit::clone() const
{
    return new PortConduit(*this);
}

void PortConduit::resolve_params(ParamResolver& r)
{
    auto subports = get_subports();
    for (auto p : subports)
    {
        p->resolve_params(r);
    }
}

Port * PortConduit::export_port(const std::string& name, NodeSystem* context)
{
	auto result = context->create_conduit_port(name, get_dir());
	auto result_impl = dynamic_cast<PortConduit*>(result);

	for (auto old_subp : get_subports())
	{
		auto& role = old_subp->get_role();

		const auto& old_bind = old_subp->get_hdl_binding();
		const auto& old_hdl_port = get_node()->get_hdl_state().get_port(old_bind.get_port_name());

		// Create a new HDL port on the system
		auto new_hdl_name = util::str_con_cat(name, role.tag);
		context->get_hdl_state().get_or_create_port(new_hdl_name, old_hdl_port->get_width(),
			old_hdl_port->get_depth(), old_hdl_port->get_dir());

		// Create a binding to this new HDL port.
		// It will have same width/depth but start at slice and bit 0
		auto new_bind = old_bind;
		new_bind.set_lo_bit(0);
		new_bind.set_lo_slice(0);
		new_bind.set_port_name(new_hdl_name);

		// Create the actual exported subport, and give it the new binding
		auto new_subp = new PortConduitSub(old_subp->get_name(), old_subp->get_dir(), role);
		new_subp->set_hdl_binding(new_bind);

		// Add subport to exported conduit port
		result_impl->add_child(new_subp);
	}

	return dynamic_cast<Port*>(result);
}

std::vector<PortConduitSub*> PortConduit::get_subports() const
{
    return get_children_by_type<PortConduitSub>();
}

PortConduitSub * PortConduit::get_subport(const std::string & tag)
{
	auto subs = get_subports();
	for (auto sub : subs)
	{
		if (sub->get_role().tag == tag)
			return sub;
	}

	return nullptr;
}

PortConduitSub * PortConduit::add_subport(const SigRoleID& role, 
	const hdl::PortBindingRef & bnd)
{
	if (role.tag.empty())
		throw Exception(get_hier_path() + ": must supply unique nonempty role tag");

	// Create logical subport, first determine its direction
	auto subport_dir = get_dir();
	if (role.type == PortConduit::IN) 
		subport_dir = Dir::IN;
	else if (role.type == PortConduit::OUT)
		subport_dir = Dir::OUT;
	else if (role.type == PortConduit::INOUT || role.type == PortConduit::FWD)
	{
		// keep it the same as conduit port's dir
	}
	else if (role.type == PortConduit::REV)
	{
		subport_dir = subport_dir.rev();
	}

	// Create the subport, set up HDL binding
	auto subport = new PortConduitSub(role.tag, subport_dir, role);
	subport->set_hdl_binding(bnd);
	add_child(subport);
	return subport;
}

//
// SubPort
//

PortType genie::impl::PORT_CONDUIT_SUB;

void PortConduitSub::init()
{
	PORT_CONDUIT_SUB = genie::impl::register_port_type(new PortConduitSubInfo());
}

PortConduitSub::PortConduitSub(const std::string & name, genie::Port::Dir dir, 
	const SigRoleID& role)
    : SubPortBase(name, dir), m_role(role)
{
	make_connectable(NET_CONDUIT_SUB);
}

PortConduitSub * PortConduitSub::clone() const
{
    return new PortConduitSub(*this);
}

Port * PortConduitSub::export_port(const std::string& name, NodeSystem* context)
{
	auto result = new PortConduitSub(name, get_dir(), get_role());
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


