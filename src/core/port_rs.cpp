#include "pch.h"
#include "port_rs.h"
#include "net_rs.h"
#include "net_topo.h"
#include "net_internal.h"
#include "port_clockreset.h"
#include "node_system.h"
#include "genie_priv.h"

using namespace genie::impl;
using genie::HDLPortSpec;
using genie::HDLBindSpec;


namespace
{
	class PortRSInfo : public PortTypeInfo
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

	class PortRSSubInfo : public PortTypeInfo
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

void PortRS::add_signal(Role role, const std::string & sig_name, const std::string & width)
{
    add_signal(role, sig_name, "", width);
}

void PortRS::add_signal(Role role, const std::string & sig_name, const std::string & tag, 
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

    add_signal_ex(role, tag, ps, bs);
}

void PortRS::add_signal_ex(Role role, const HDLPortSpec& ps, const HDLBindSpec& bs)
{
    add_signal_ex(role, "", ps, bs);
}

void PortRS::add_signal_ex(Role role, const std::string & tag, const HDLPortSpec& pspec, 
    const HDLBindSpec& bspec)
{
    auto& hdl_state = get_node()->get_hdl_state();

    // Determine required HDL port dir form role.
    // Assume it's the same as the RSPort, and invert for READY
    hdl::Port::Dir hdl_dir = hdl::Port::Dir::from_logical(get_dir());
	if (role == Role::READY)
		hdl_dir = hdl_dir.rev();

    // Get or create HDL port
    auto& hdl_port = hdl_state.get_or_create_port(pspec.name, pspec.width, pspec.depth, hdl_dir);

    hdl::PortBindingRef hdl_pb;
    hdl_pb.set_port_name(pspec.name);
    hdl_pb.set_bits(bspec.width);
    hdl_pb.set_lo_bit(bspec.lo_bit);
    hdl_pb.set_slices(bspec.depth);
    hdl_pb.set_lo_slice(bspec.lo_slice);

	add_subport(role, tag, hdl_pb);
}

//
// Internal
//

PortType genie::impl::PORT_RS;

void PortRS::init()
{
	PORT_RS = genie::impl::register_port_type(new PortRSInfo());
}

PortRS::PortRS(const std::string & name, genie::Port::Dir dir)
    : Port(name, dir)
{
	make_connectable(NET_RS_LOGICAL);
	make_connectable(NET_RS);
	make_connectable(NET_TOPO);
	make_connectable(NET_INTERNAL, dir.rev());
}

PortRS::PortRS(const std::string & name, genie::Port::Dir dir, 
	const std::string & clk_port_name)
	: PortRS(name, dir)
{
	set_clock_port_name(clk_port_name);
}

PortRS::PortRS(const PortRS &o)
    : Port(o), m_clk_port_name(o.m_clk_port_name)
{
}

PortRS::~PortRS()
{
}

Port * PortRS::instantiate() const
{
    return new PortRS(*this);
}

void PortRS::resolve_params(ParamResolver& r)
{
    auto ports = get_subports();
    for (auto p : ports)
        p->resolve_params(r);
}

genie::Port * PortRS::export_port(const std::string & name, NodeSystem* context)
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
	for (auto old_subp : get_subports())
	{
		auto role = old_subp->get_role();
		auto tag = old_subp->get_tag();

		const auto& old_bind = old_subp->get_hdl_binding();
		const auto& old_hdl_port = get_node()->get_hdl_state().get_port(old_bind.get_port_name());

		// Create a new HDL port on the system
		auto new_hdl_name = util::str_con_cat(name, tag);
		context->get_hdl_state().get_or_create_port(new_hdl_name, old_hdl_port->get_width(),
			old_hdl_port->get_depth(), old_hdl_port->get_dir());

		// Create a binding to this new HDL port.
		// It will have same width/depth but start at slice and bit 0
		auto new_bind = old_bind;
		new_bind.set_lo_bit(0);
		new_bind.set_lo_slice(0);
		new_bind.set_port_name(new_hdl_name);

		// Create the actual exported subport, and give it the new binding
		auto new_subp = new PortRSSub(old_subp->get_name(), old_subp->get_dir());
		new_subp->set_role(role);
		new_subp->set_tag(tag);
		new_subp->set_hdl_binding(new_bind);

		// Add subport to exported conduit port
		result_impl->add_child(new_subp);
	}

	return result;
}

std::vector<PortRSSub*> PortRS::get_subports() const
{
    return get_children_by_type<PortRSSub>();
}

std::vector<PortRSSub*> PortRS::get_subports(genie::PortRS::Role role)
{
	auto result = get_children<PortRSSub>([=](const PortRSSub* o)
	{
		return o->get_role() == role;
	});

	return result;
}

PortRSSub *PortRS::get_subport(genie::PortRS::Role role, const std::string & tag)
{
	auto children = get_children_by_type<PortRSSub>();
	for (auto sub : children)
	{
		if (sub->get_role() == role && sub->get_tag() == tag)
			return sub;
	}

	return nullptr;
}

PortRSSub * PortRS::add_subport(genie::PortRS::Role role, const hdl::PortBindingRef & bnd)
{
	return add_subport(role, "", bnd);
}

PortRSSub * PortRS::add_subport(genie::PortRS::Role role, const std::string & tag, 
	const hdl::PortBindingRef & bnd)
{
	// Check whether tag is required based on role
	switch (role)
	{
	case Role::DATABUNDLE:
		if (tag.empty())
			throw Exception(get_hier_path() + ": role " + role.to_string() + " requires a tag");
		break;

	default:
		if (!tag.empty())
			throw Exception(get_hier_path() + ": role " + role.to_string() + " must have empty tag");
		break;
	}

	// Create subport, figure out direction and name
	auto subport_dir = get_dir();
	std::string subport_name = role.to_string();

	switch (role)
	{
	case Role::READY:
		subport_dir = subport_dir.rev();
		break;
	case Role::DATABUNDLE:
		subport_name = util::str_con_cat("data", tag);
		break;
	}

	util::str_makelower(subport_name);

	// Create the subport
	auto subport = new PortRSSub(subport_name, subport_dir);
	subport->set_role(role);
	subport->set_tag(tag);
	subport->set_hdl_binding(bnd);

	add_child(subport);
	return subport;
}

PortClock * PortRS::get_clock_port() const
{
	auto obj = get_node()->get_child(get_clock_port_name());
	return dynamic_cast<PortClock*>(obj);
}

//
// SubPort
//

PortType genie::impl::PORT_RS_SUB;

void PortRSSub::init()
{
	PORT_RS_SUB = genie::impl::register_port_type(new PortRSSubInfo());
}

PortRSSub::PortRSSub(const std::string & name, genie::Port::Dir dir)
    : SubPortBase(name, dir)
{
	make_connectable(NET_RS_SUB);
}

PortRSSub::PortRSSub(const PortRSSub &o)
    : SubPortBase(o)
{
}

Port * PortRSSub::instantiate() const
{
    return new PortRSSub(*this);
}

void PortRSSub::resolve_params(ParamResolver& r)
{
    m_binding.resolve_params(r);
}

genie::Port* PortRSSub::export_port(const std::string & name, NodeSystem* context)
{
	auto result = new PortRSSub(name, get_dir());
	result->set_tag(get_tag());

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


