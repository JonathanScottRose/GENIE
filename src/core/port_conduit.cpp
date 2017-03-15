#include "pch.h"
#include "port_conduit.h"
#include "genie/port.h"

using namespace genie::impl;

//
// Public
//

void PortConduit::add_signal(Role role, const std::string & tag, 
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
    
    add_signal(role, tag, ps, bs);
}

void PortConduit::add_signal(Role role, const std::string & tag, 
    const genie::HDLPortSpec &pspec, const genie::HDLBindSpec &bspec)
{
    if (tag.empty())
        throw Exception(get_hier_path() + ": must supply unique nonempty role tag");

    // Get or create HDL port
    auto& hdl_state = get_node()->get_hdl_state();

    hdl::Port::Dir hdl_dir = hdl::Port::Dir::INOUT;
    switch (role)
    {
	case Role::IN: hdl_dir = hdl::Port::Dir::IN; break;
	case Role::OUT: hdl_dir = hdl::Port::Dir::OUT; break;
	case Role::INOUT: hdl_dir = hdl::Port::Dir::INOUT; break;
	case Role::FWD: hdl_dir = hdl::Port::Dir::from_logical(get_dir()); break;
    case Role::REV: 
        hdl_dir = hdl::Port::Dir::from_logical(get_dir());
		hdl_dir = hdl_dir.rev();
        break;
    default:
        assert(false);
    }

    auto& hdl_port = hdl_state.get_or_create_port(pspec.name, pspec.width, pspec.depth, hdl_dir);

    // Create logical subport, first determine its direction
    auto subport_dir = get_dir();
    switch (role)
    {
    case Role::IN: subport_dir = Dir::IN; break;
    case Role::OUT: subport_dir = Dir::OUT; break;
    case Role::INOUT:
    case Role::FWD:
        // keep it the same as conduit port's dir
        break;
	case Role::REV: 
		subport_dir = subport_dir.rev();
        break;
    }

    // Create the subport, set up HDL binding
    auto subport = new PortConduitSub(tag, subport_dir);
    
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

PortConduit::PortConduit(const std::string & name, genie::Port::Dir dir)
    : Port(name, dir)
{
}

PortConduit::PortConduit(const PortConduit &o)
    : Port(o)
{
    // Copy role-things?
}

PortConduit::~PortConduit()
{
}

Port * PortConduit::instantiate() const
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

std::vector<PortConduitSub*> PortConduit::get_subports() const
{
    return get_children_by_type<PortConduitSub>();
}

//
// SubPort
//

PortConduitSub::PortConduitSub(const std::string & name, genie::Port::Dir dir)
    : SubPortBase(name, dir)
{
}

Port * PortConduitSub::instantiate() const
{
    return new PortConduitSub(*this);
}


