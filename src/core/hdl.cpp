#include "pch.h"
#include "hdl.h"
#include "node.h"
#include "bits_val.h"
#include "int_expr.h"
#include "params.h"

using namespace genie::impl::hdl;
using genie::impl::IntExpr;
using genie::impl::BitsVal;
using genie::impl::Node;
using genie::impl::ParamResolver;

//
// Port
//

Port::Port(const std::string& name)
    : m_parent(nullptr), m_name(name)
{
}

Port::Port(const Port& o)
    : m_name(o.m_name), m_width(o.m_width), m_depth(o.m_depth), m_dir(o.m_dir), 
    m_bindings(o.m_bindings)
{
    for (auto& b : m_bindings)
    {
        b.set_parent(this);
    }
}

Port::Port(Port&& o)
    : m_name(std::move(o.m_name)), m_width(std::move(o.m_width)), m_depth(std::move(o.m_depth)),
    m_dir(o.m_dir), m_bindings(std::move(o.m_bindings))
{
    for (auto& b : m_bindings)
    {
        b.set_parent(this);
    }

    o.m_parent = nullptr;
}

Port::Port(const std::string& name, const IntExpr& width, const IntExpr& depth, Dir dir)
    : m_parent(nullptr), m_name(name), m_width(width), m_depth(depth), m_dir(dir)
{
}

Port::~Port()
{
}

Port::Dir Port::Dir::rev() const
{
	Port::Dir d = *this;
    switch (d)
    {
    case IN: return OUT;
    case OUT: return IN;
    case INOUT: return INOUT;
    default: assert(false);
    }

    return INOUT;
}

Port::Dir Port::Dir::from_logical(genie::Port::Dir dir)
{
    switch (dir)
    {
	case genie::Port::Dir::IN: return Port::Dir::IN;
	case genie::Port::Dir::OUT: return Port::Dir::OUT;
    default: assert(false);
    }

    return Port::Dir::INOUT;
}


/*
Port::Dir Port::make_dir(genie::Dir pdir, genie::SigRole::Sense sense)
{
    // Convert a Port's direction (IN or OUT) plus a Signal Role's sense (FWD, REV, IN, OUT, INOUT)
    // into a concrete Verilog port direction.
    Port::Dir result;

    switch (pdir)
    {
    case genie::Dir::IN: result = Port::IN; break;
    case genie::Dir::OUT: result = Port::OUT; break;
    default: assert(false);
    }

    switch (sense)
    {
    case SigRole::FWD: break;
    case SigRole::REV: result = rev_dir(result); break;
    case SigRole::IN: result = Port::IN; break;
    case SigRole::OUT: result = Port::OUT; break;
    case SigRole::INOUT: result = Port::INOUT; break;
    default: assert(false);
    }

    return result;
}*/

void Port::resolve_params(ParamResolver& resolv)
{
    m_width.evaluate(resolv);
    m_depth.evaluate(resolv);
}

bool Port::is_bound() const
{
    return !m_bindings.empty();
}

void Port::bind(Bindable* target, 
    unsigned bind_dim, int bind_size, 
    int targ_slice, int targ_bit, 
    int port_slice, int port_bit)
{
    assert( (bind_dim == 0) || (bind_dim == 1 && targ_bit == 0 && port_bit == 0) );

    // The incoming binding
    int new_slice_size = bind_dim == 0 ? 1 : bind_size;
    int new_bit_size = bind_dim == 0 ? bind_size : m_width;
    int new_slice = port_slice;
    int new_bit = port_bit;

    // Check if this binding will overlap with existing bindings (at the port binding site)
    for (auto& b : m_bindings)
    {
        bool slice_overlap = 
            new_slice + new_slice_size > b.get_port_lo_slice() &&
            b.get_port_lo_slice() + b.get_bound_slices() > new_slice;

        bool bit_overlap =
            new_bit + new_bit_size > b.get_port_lo_bit() &&
            b.get_port_lo_bit() + b.get_bound_bits() > new_bit;

        if (slice_overlap || bit_overlap)
        {
            throw Exception("binding overlap. more descriptive message TODO");
        }
    }

    // Create new binding
    m_bindings.emplace_back(this, target);
    auto& bnd = m_bindings.back();

    bnd.set_target_lo_slice(targ_slice);
    bnd.set_target_lo_bit(targ_bit);
    bnd.set_port_lo_slice(port_slice);
    bnd.set_port_lo_bit(port_bit);
    bnd.set_bound_slices(new_slice_size);
    bnd.set_bound_bits(new_bit_size);
}

//
// PortBinding
//

PortBinding::PortBinding(Port* parent, Bindable* target)
    : m_parent(parent), m_target(target)
{
}

PortBinding::~PortBinding()
{
}

bool PortBinding::is_full1_port_binding() const
{
    return m_port_lo_slice == 0 && m_bound_slices == m_parent->get_depth();
}

bool PortBinding::is_full1_target_binding() const
{
    return m_target_lo_slice == 0 && m_bound_slices == m_target->get_depth();
}

bool PortBinding::is_full0_port_binding() const
{
    return m_port_lo_bit == 0 && m_bound_bits == m_parent->get_width();
}

bool PortBinding::is_full0_target_binding() const
{
    return m_target_lo_bit == 0 && m_bound_bits == m_target->get_width();
}

//
// ConstValue
//

ConstValue::ConstValue(const BitsVal& v)
    : m_value(v)
{
}

int ConstValue::get_width() const
{
    return m_value.get_size(0);
}

int ConstValue::get_depth() const
{
    return m_value.get_size(1);
}

//
// Net
//

Net::Net(Type type, const std::string& name)
    : m_type(type), m_name(name)
{
}

Net::~Net()
{
}

int Net::get_width() const
{
    return m_width;
}

void Net::set_width(int width)
{
    m_width = width;
}

int Net::get_depth() const
{
    return m_depth;
}

void Net::set_depth(int depth)
{
    m_depth = depth;
}

//
// HDLState
//

HDLState::HDLState(Node* n)
    : m_node(n)
{
}

HDLState::HDLState(const HDLState &o)
    : m_ports(o.m_ports), m_node(nullptr)
{
    // Only ports copied. Change their parent to point back to us
    for (auto& p : m_ports)
        p.second.set_parent(this);
}

HDLState::~HDLState()
{
}

void HDLState::resolve_params(ParamResolver& resolv)
{
    for (auto& p: m_ports)
        p.second.resolve_params(resolv);
}

Port & HDLState::get_or_create_port(const std::string & name, const IntExpr & width, 
    const IntExpr & depth, Port::Dir dir)
{
    Port* result = get_port(name);
    if (!result)
    {
        return add_port(name, width, depth, dir);
    }
    //else

    // validate
    if (width != result->get_width())
    {
        throw Exception(get_node()->get_hier_path() + ": existing port " + name + 
            " width of " + width.to_string() + " differs from given width of " +
            width.to_string());
    }

    if (depth != result->get_depth())
    {
        throw Exception(get_node()->get_hier_path() + ": existing port " + name + 
            " depth of " + depth.to_string() + " differs from given depth of " +
            depth.to_string());
    }

    return *result;
}

Port& HDLState::add_port(const std::string& name)
{
    if (util::exists_2(m_ports, name))
    {
        throw Exception(m_node->get_hier_path() + " already has an HDL port named "
            + name);
    }

    auto it = m_ports.emplace(typename decltype(m_ports)::value_type(name, Port(name)));
    auto& new_port = it.first->second;

    new_port.set_parent(this); 
    return new_port;   
}

Port& HDLState::add_port(const std::string& name, const IntExpr& width, 
    const IntExpr& depth, Port::Dir dir)
{
    if (util::exists_2(m_ports, name))
    {
        throw Exception(m_node->get_hier_path() + " already has an HDL port named "
            + name);
    }

    auto it = m_ports.emplace(typename decltype(m_ports)::value_type(name, 
        Port(name, width, depth, dir)));
    auto& new_port = it.first->second;

    new_port.set_parent(this); 
    return new_port;   
}

Net& HDLState::add_net(Net::Type type, const std::string & name)
{
    if (util::exists_2(m_nets, name))
    {
        throw Exception(m_node->get_hier_path() + " already has net named "
            + name);
    }

    auto it = m_nets.emplace(typename decltype(m_nets)::value_type(name, 
        Net(type, name)));
    auto& new_net = it.first->second;

    return new_net;   
}

Port * HDLState::get_port(const std::string& name)
{
    auto it = m_ports.find(name);
    return it == m_ports.end()? nullptr : &it->second;
}

Net * HDLState::get_net(const std::string& name)
{
    auto it = m_nets.find(name);
    return it == m_nets.end()? nullptr : &it->second;
}

auto HDLState::get_ports() const -> const decltype(m_ports)& 
{
    return m_ports;
}

auto HDLState::get_nets() const -> const decltype(m_nets)& 
{
    return m_nets;
}

void HDLState::connect(const std::string& src_name, const std::string& sink_name, 
    int src_slice, int src_lsb, int sink_slice, int sink_lsb, unsigned dim, int size)
{
    // Get the ports by name
    Port* src = get_port(src_name);
    Port* sink = get_port(sink_name);

    assert(src);
    assert(sink);

    // Create or grab a net that's bound to the entire src.
    // This process depends on whether the Port is top-level to this System or not.
    bool src_is_export = src->get_parent() == this;
    bool sink_is_export = sink->get_parent() == this;

    // Validate directions of src/sink
    auto src_effective_dir = src->get_dir();
    auto sink_effective_dir = sink->get_dir();

	if (src_is_export) src_effective_dir = src_effective_dir.rev();
    if (sink_is_export) sink_effective_dir = sink_effective_dir.rev();

    assert(src_effective_dir != Port::Dir::IN);
    assert(sink_effective_dir != Port::Dir::OUT);

    // This is the name of the net. Non-export nets are named nodename_portname and exports are
    // just named exactly after the portname.
    std::string src_netname = src_is_export? 
        src->get_name() : util::str_con_cat(src->get_parent()->get_node()->get_name(), src->get_name());

    // Try to find existing one
    Net* net = get_net(src_netname);
    if (!net)
    {
        // Not found? Create and bind to src.
        int src_width = src->get_width();
        int src_depth = src->get_depth();

        auto nettype = src_is_export? Net::EXPORT : Net::WIRE;
        net = &add_net(nettype, src_netname);
        net->set_width(src_width);
        net->set_depth(src_depth);

        // Bind net to entire src (all slices)
        src->bind(net, 1, src_depth, 0, 0, 0, 0);
    }

    // Now bind (part of) the net to the sink
    sink->bind(net, dim, size, sink_slice, sink_lsb, src_slice, src_lsb);
}

void HDLState::connect(const std::string& sink_name, const BitsVal& val, int sink_slice, int sink_lsb)
{
    Port* sink = get_port(sink_name);

    // Create a new ConstValue bindable object
    m_const_values.emplace_back(val);
    auto& cv = m_const_values.back();

    // Bind it to the sink
    //
    // Check the dimensionality of the value to see if we're binding bits in a slice or entire slices.
    unsigned dim = val.get_size(1) > 1;
    unsigned size = val.get_size(dim);

    sink->bind(&cv, dim, size, 0, 0, sink_slice, sink_lsb);
}

void HDLState::connect(const PortBindingRef & src, const PortBindingRef & sink)
{
    assert(src.is_resolved());
    assert(sink.is_resolved());

    assert(src.get_slices() == sink.get_slices());
    int slices = src.get_slices();

    unsigned dim;
    int size;
    if (slices > 1)
    {
        dim = 1;
        size = slices;
        assert(src.get_bits() == sink.get_bits());
    }
    else
    {
        dim = 0;
        size = std::min(src.get_bits(), sink.get_bits());
    }

    connect(src.get_port_name(), sink.get_port_name(), src.get_lo_slice(), src.get_lo_bit(),
        sink.get_lo_slice(), sink.get_lo_bit(), dim, size);
}

void HDLState::connect(const PortBindingRef& sink, const BitsVal& val)
{
    connect(sink.get_port_name(), val, sink.get_lo_slice(), sink.get_lo_bit());
}

//
// PortBindingRef
//

PortBindingRef::PortBindingRef()
    : m_slices(1), m_lo_slice(0), m_bits(1), m_lo_bit(0)
{
}

void PortBindingRef::resolve_params(ParamResolver& r)
{
    m_slices.evaluate(r);
    m_lo_bit.evaluate(r);
    m_lo_slice.evaluate(r);
    m_slices.evaluate(r);
}

bool PortBindingRef::is_resolved() const
{
    return m_bits.is_const() && 
        m_lo_bit.is_const() &&
        m_lo_slice.is_const() &&
        m_slices.is_const();
}

std::string PortBindingRef::to_string() const
{
    std::string result = m_port_name;

    // Add optional [foo:bar] for slices and bits size
    for (auto& p : {std::make_pair(m_lo_slice, m_slices), std::make_pair(m_lo_bit, m_bits)})
    {
        const IntExpr& lo = p.first;
        const IntExpr& size = p.second;

        if (lo.is_const() && size.is_const())
        {
            if (size > 1 && lo > 0)
            {
                int hi = lo + size - 1;
                result += "[" + std::to_string(hi) + ":" + std::to_string(lo) + "]";
            }
        }
        else
        {
            result += "[" + lo.to_string() + "+" + size.to_string() + "-1:" + 
                lo.to_string() + "]";
        }
    }

    return result;
}

