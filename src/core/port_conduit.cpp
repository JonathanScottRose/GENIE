#include "pch.h"
#include "port_conduit.h"
#include "net_conduit.h"
#include "node_system.h"
#include "genie_priv.h"
#include "genie/port.h"

using namespace genie::impl;

namespace
{
	class PortConduitInfo : public PortTypeInfo
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

	class PortConduitSubInfo : public PortTypeInfo
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
	auto subport = new PortConduitSub(tag, subport_dir, role, tag);
    
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

PortType genie::impl::PORT_CONDUIT;

void PortConduit::init()
{
	PORT_CONDUIT = genie::impl::register_port_type(new PortConduitInfo());
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

genie::Port * PortConduit::export_port(const std::string& name, NodeSystem* context)
{
	auto result = context->create_conduit_port(name, get_dir());
	auto result_impl = dynamic_cast<PortConduit*>(result);

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
		auto new_subp = new PortConduitSub(old_subp->get_name(), old_subp->get_dir(), role, tag);
		new_subp->set_hdl_binding(new_bind);

		// Add subport to exported conduit port
		result_impl->add_child(new_subp);
	}

	return result;
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
		if (sub->get_tag() == tag)
			return sub;
	}

	return nullptr;
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
	genie::PortConduit::Role role, const std::string& tag)
    : SubPortBase(name, dir), m_tag(tag), m_role(role)
{
	make_connectable(NET_CONDUIT_SUB);
}

Port * PortConduitSub::instantiate() const
{
    return new PortConduitSub(*this);
}

genie::Port * PortConduitSub::export_port(const std::string& name, NodeSystem* context)
{
	auto result = new PortConduitSub(name, get_dir(), get_role(), get_tag());
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


