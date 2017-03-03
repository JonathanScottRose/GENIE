#include "pch.h"
#include "port_rs.h"

using namespace genie::impl;
using genie::HDLPortSpec;
using genie::HDLBindSpec;

//
// Public
//

void RSPort::add_signal(Role role, const std::string & sig_name, const std::string & width)
{
    add_signal(role, sig_name, "", width);
}

void RSPort::add_signal(Role role, const std::string & sig_name, const std::string & tag, 
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

    add_signal(role, tag, ps, bs);
}

void RSPort::add_signal(Role role, const HDLPortSpec& ps, const HDLBindSpec& bs)
{
    add_signal(role, "", ps, bs);
}

void RSPort::add_signal(Role role, const std::string & tag, const HDLPortSpec& pspec, 
    const HDLBindSpec& bspec)
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

    auto& hdl_state = get_node()->get_hdl_state();

    // Determine required HDL port dir form role.
    // Assume it's the same as the RSPort, and invert for READY
    hdl::Port::Dir hdl_dir = hdl::Port::from_logical_dir(get_dir());
    if (role == Role::READY)
        hdl_dir = hdl::Port::rev_dir(hdl_dir);

    // Get or create HDL port
    auto& hdl_port = hdl_state.get_or_create_port(pspec.name, pspec.width, pspec.depth, hdl_dir);

    // Create subport, figure out direction and name
    auto subport_dir = get_dir();
    std::string subport_name = role.to_string();

    switch(role)
    {
    case Role::READY:
        subport_dir = Port::rev_dir(subport_dir);
        break;
    case Role::DATABUNDLE:
        subport_name = util::str_con_cat("data", tag);
        break;
    }

    util::str_makelower(subport_name);

    // Create the subport, set up HDL binding
    auto subport = new FieldPort(subport_name, subport_dir);

    hdl::PortBindingRef hdl_pb;
    hdl_pb.set_port_name(pspec.name);
    hdl_pb.set_bits(bspec.width);
    hdl_pb.set_lo_bit(bspec.lo_bit);
    hdl_pb.set_slices(bspec.depth);
    hdl_pb.set_lo_slice(bspec.lo_slice);

    subport->set_hdl_binding(hdl_pb);
    add_child(subport);
}

//
// Internal
//

RSPort::RSPort(const std::string & name, genie::Port::Dir dir)
    : Port(name, dir)
{
}

RSPort::RSPort(const RSPort &o)
    : Port(o), m_clk_port_name(o.m_clk_port_name)
{
}

RSPort::~RSPort()
{
}

Port * RSPort::instantiate() const
{
    return new RSPort(*this);
}

void RSPort::resolve_params(ParamResolver& r)
{
    auto ports = get_field_ports();
    for (auto p : ports)
        p->resolve_params(r);
}

std::vector<FieldPort*> RSPort::get_field_ports() const
{
    return get_children_by_type<FieldPort>();
}

//
// FieldPort
//

FieldPort::FieldPort(const std::string & name, genie::Port::Dir dir)
    : SubPortBase(name, dir)
{
}

FieldPort::FieldPort(const FieldPort &o)
    : SubPortBase(o)
{
}

Port * FieldPort::instantiate() const
{
    return new FieldPort(*this);
}

void FieldPort::resolve_params(ParamResolver& r)
{
    m_binding.resolve_params(r);
}


