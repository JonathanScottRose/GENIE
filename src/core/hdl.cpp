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
// PortBindingTarget
//

bool PortBindingTarget::is_const() const
{
	assert(m_type != UNSET);
	return m_type == CONST;
}

int PortBindingTarget::get_width(const HDLState& hdls) const
{
	switch (m_type)
	{
	case CONST: return (int)m_const.get_size(0); break;
	case NET:
	{
		auto net = hdls.get_net(m_net);
		assert(net);
		return net->get_width();
		break;
	}
	case UNSET: 
	default:
		assert(false); break;
	}

	return -1;
}

int PortBindingTarget::get_depth(const HDLState& hdls) const
{
	switch (m_type)
	{
	case CONST: return (int)m_const.get_size(1); break;
	case NET:
	{
		auto net = hdls.get_net(m_net);
		assert(net);
		return net->get_depth();
		break;
	}
	case UNSET:
	default: 
		assert(false); break;
	}

	return -1;
}

const BitsVal & PortBindingTarget::get_const() const
{
	assert(m_type == CONST);
	return m_const;
}

const std::string & PortBindingTarget::get_net_name() const
{
	assert(m_type == NET);
	return m_net;
}

PortBindingTarget::PortBindingTarget()
	: m_type(UNSET)
{
}

PortBindingTarget::PortBindingTarget(const BitsVal &val)
	: m_const(val), m_type(CONST)
{
}

PortBindingTarget::PortBindingTarget(const std::string &name)
	: m_net(name), m_type(NET)
{
}

PortBindingTarget::PortBindingTarget(const PortBindingTarget &o)
	: m_type(UNSET)
{
	*this = o;
}

PortBindingTarget & PortBindingTarget::operator=(const PortBindingTarget &o)
{
	if (m_type != UNSET)
		this->~PortBindingTarget();

	m_type = o.m_type;

	switch (m_type)
	{
	case NET:
		new (&m_net) std::string(o.m_net);
		break;

	case CONST:
		new (&m_const) BitsVal(o.m_const);
		break;

	case UNSET:
	default:
		break;
	}

	return *this;
}

PortBindingTarget::~PortBindingTarget()
{
	switch (m_type)
	{
	case NET:
		m_net.~basic_string();
		break;

	case CONST:
		m_const.~BitsVal();
		break;

	case UNSET:
	default:
		break;
	}

	m_type = UNSET;
}



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

void Port::bind(const PortBindingTarget& target, 
    unsigned bind_dim, int bind_size, 
    int targ_slice, int targ_bit, 
    int port_slice, int port_bit)
{
    assert( (bind_dim == 0) || (bind_dim == 1 && targ_bit == 0 && port_bit == 0) );

    // The incoming binding
    int new_slice_size = bind_dim == 0 ? 1 : (int)bind_size;
    int new_bit_size = bind_dim == 0 ? bind_size : (int)m_width;
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

		// Needs to be both to be bad.
		assert(!(slice_overlap && bit_overlap));
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

PortBinding::PortBinding(Port* parent, const PortBindingTarget& target)
    : m_parent(parent), m_target(target)
{
}

bool PortBinding::is_full1_port_binding() const
{
    return m_port_lo_slice == 0 && m_bound_slices == m_parent->get_depth();
}

bool PortBinding::is_full1_target_binding(const HDLState& context) const
{
    return m_target_lo_slice == 0 && m_bound_slices == m_target.get_depth(context);
}

bool PortBinding::is_full0_port_binding() const
{
    return m_port_lo_bit == 0 && m_bound_bits == m_parent->get_width();
}

bool PortBinding::is_full0_target_binding(const HDLState& context) const
{
    return m_target_lo_bit == 0 && m_bound_bits == m_target.get_width(context);
}

//
// Net
//

Net::Net(Type type, const std::string& name)
    : m_type(type), m_name(name)
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
    : m_ports(o.m_ports), m_nets(o.m_nets)
{
	update_parent_refs();
}

HDLState::HDLState(HDLState&& o)
	: m_ports(std::move(o.m_ports)), m_nets(std::move(o.m_nets))
{
	update_parent_refs();
}

HDLState::~HDLState()
{
}

HDLState & HDLState::operator=(const HDLState &o)
{
	m_ports = o.m_ports;
	m_nets = o.m_nets;
	update_parent_refs();
	return *this;
}

HDLState & HDLState::operator=(HDLState &&o)
{
	m_ports = std::move(o.m_ports);
	m_nets = std::move(o.m_nets);
	update_parent_refs();
	return *this;
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
    if (!width.equals(result->get_width()))
    {
        throw Exception(get_node()->get_hier_path() + ": existing port " + name + 
            " width of " + width.to_string() + " differs from given width of " +
            width.to_string());
    }

    if (!depth.equals(result->get_depth()))
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

void HDLState::update_parent_refs()
{
	// Point ports back
	for (auto& p : m_ports)
		p.second.set_parent(this);
}

Port * HDLState::get_port(const std::string& name)
{
    auto it = m_ports.find(name);
    return it == m_ports.end()? nullptr : &it->second;
}

Net * HDLState::get_net(const std::string& name) const
{
    auto it = m_nets.find(name);
	return it == m_nets.end() ? nullptr : const_cast<Net*>(&it->second);
}

auto HDLState::get_ports() const -> const decltype(m_ports)& 
{
    return m_ports;
}

auto HDLState::get_nets() const -> const decltype(m_nets)& 
{
    return m_nets;
}

void HDLState::connect(Port* src, Port* sink, 
    int src_slice, int src_lsb, int sink_slice, int sink_lsb, unsigned dim, int size)
{
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
        src->bind(src_netname, 1, src_depth, 0, 0, 0, 0);
    }

    // Now bind (part of) the net to the sink
	sink->bind(src_netname, dim, size, src_slice, src_lsb, sink_slice, sink_lsb);
}

void HDLState::connect(Port* sink, const BitsVal& val, int sink_slice, int sink_lsb)
{
    // Bind it to the sink
    //
    // Check the dimensionality of the value to see if we're binding bits in a slice or entire slices.
    unsigned dim = val.get_size(1) > 1;
    unsigned size = val.get_size(dim);

    sink->bind(val, dim, size, 0, 0, sink_slice, sink_lsb);
}

void HDLState::connect(Node* src_node, const PortBindingRef & src, 
	Node* sink_node, const PortBindingRef & sink)
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
		// TODO: enforce that binding bits == port bits for both ports
    }
    else
    {
        dim = 0;
        size = std::min(src.get_bits(), sink.get_bits());
    }

	auto src_port = src_node->get_hdl_state().get_port(src.get_port_name());
	auto sink_port = sink_node->get_hdl_state().get_port(sink.get_port_name());

	assert(src_port);
	assert(sink_port);

    connect(src_port, sink_port, src.get_lo_slice(), src.get_lo_bit(),
        sink.get_lo_slice(), sink.get_lo_bit(), dim, size);
}

void HDLState::connect(Node* node, const PortBindingRef& sink, const BitsVal& val)
{
	auto port = node->get_hdl_state().get_port(sink.get_port_name());
	assert(port);

    connect(port, val, sink.get_lo_slice(), sink.get_lo_bit());
}

//
// PortBindingRef
//

PortBindingRef::PortBindingRef()
    : m_slices(1), m_lo_slice(0), m_bits(1), m_lo_bit(0)
{
}

PortBindingRef::PortBindingRef(const std::string & name)
	: PortBindingRef(name.c_str())
{
}

PortBindingRef::PortBindingRef(const std::string & name, const IntExpr& bits)
	: PortBindingRef(name.c_str(), bits)
{
}

PortBindingRef::PortBindingRef(const std::string & name, const IntExpr& bits,
	const IntExpr& slices)
	: PortBindingRef(name.c_str(), bits, slices)
{
}

PortBindingRef::PortBindingRef(const char * name)
	: m_port_name(name), m_slices(1), m_lo_slice(0), m_bits(1), m_lo_bit(0)
{
}

PortBindingRef::PortBindingRef(const char * name, const IntExpr & bits)
	: m_port_name(name), m_slices(1), m_lo_slice(0), m_bits(bits), m_lo_bit(0)
{
}

PortBindingRef::PortBindingRef(const char * name, const IntExpr & bits, const IntExpr & slices)
	: m_port_name(name), m_slices(slices), m_lo_slice(0), m_bits(bits), m_lo_bit(0)
{
}

PortBindingRef & PortBindingRef::set_lo_bit(const IntExpr& val)
{
	m_lo_bit = val;
	return *this;
}

PortBindingRef & PortBindingRef::set_lo_slice(const IntExpr& val)
{
	m_lo_slice = val;
	return *this;
}

void PortBindingRef::resolve_params(ParamResolver& r)
{
    m_bits.evaluate(r);
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

