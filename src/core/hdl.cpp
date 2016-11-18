#include "genie/common.h"
#include "genie/hdl.h"
#include "genie/structure.h"
#include "genie/value.h"

using namespace genie::hdl;

//
// Port
//

Port::Port(const std::string& name)
	: m_parent(nullptr), m_name(name)
{
}

Port::Port(const std::string& name, const Expression& width, Dir dir)
	: m_parent(nullptr), m_name(name), m_width(width), m_dir(dir)
{
}

Port* Port::instantiate() const
{
	return new Port(m_name, m_width, m_dir);
}

Port::~Port()
{
	util::delete_all(m_bindings);
}

Port::Dir Port::rev_dir(Port::Dir in)
{
	switch (in)
	{
		case IN: return OUT;
		case OUT: return IN;
		case INOUT: return INOUT;
	}

	assert(false);
	return INOUT;
}

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
}

bool Port::is_bound()
{
	return !m_bindings.empty();
}

int Port::eval_width()
{
	// Concrete-ize our width by plugging in parameter definitions from the parent genie::Node
	Node* node = get_parent()->get_node();
	assert(node);

	return m_width.get_value(node->get_exp_resolver());
}

void Port::bind(Bindable* target, int width, int port_lsb, int targ_lsb)
{
	// Check if this binding will overlap with existing bindings (at the port binding site)
	for (auto& b : m_bindings)
	{
		assert(port_lsb + width <= b->get_port_lsb() ||
			port_lsb >= b->get_port_lsb() + b->get_width());
	}

	int port_width = eval_width();
	int targ_width = target->get_width();
	int port_hi = port_lsb + width - 1;
	int targ_hi = targ_lsb + width - 1;
	
	assert(port_hi < port_lsb + port_width);
	assert(targ_hi < targ_lsb + targ_width);

	// Create the binding and add it to our list of bindings
	PortBinding* binding = new PortBinding(this, target);
	binding->set_port_lsb(port_lsb);
	binding->set_target_lsb(targ_lsb);
	binding->set_width(width);
	m_bindings.push_back(binding);
}

void Port::bind(Bindable* target, int port_lsb, int targ_lsb)
{
	// Bind the full width of the target. 
	bind(target, target->get_width(), port_lsb, targ_lsb);
}

//
// NodeHDLInfo
//

NodeHDLInfo::NodeHDLInfo(const std::string& name)
	: m_mod_name(name)
{
}

NodeHDLInfo::~NodeHDLInfo()
{
	util::delete_all_2(m_ports);
    util::delete_all(m_const_values);
    util::delete_all_2(m_nets);
}

Port* NodeHDLInfo::add_port(Port* p)
{
	assert(!util::exists_2(m_ports, p->get_name()));
	m_ports[p->get_name()] = p;
	p->set_parent(this);
	return p;
}


NodeHDLInfo* NodeHDLInfo::instantiate()
{
	// Use same module name
	auto result = new NodeHDLInfo(m_mod_name);

	// Copy all Ports
	for (auto& i : ports())
	{
		result->add_port(i.second->instantiate());
	}

	return result;
}

void NodeHDLInfo::connect(Port* src, Port* sink, int src_lsb, int sink_lsb, int width)
{
    // Create or grab a net that's bound to the entire src.
    // This process depends on whether the Port is top-level to this System or not.
    bool src_is_export = src->get_parent() == this;
    bool sink_is_export = sink->get_parent() == this;

    // Validate directions of src/sink
    auto src_effective_dir = src->get_dir();
    auto sink_effective_dir = sink->get_dir();

    if (src_is_export) src_effective_dir = Port::rev_dir(src_effective_dir);
    if (sink_is_export) sink_effective_dir = Port::rev_dir(sink_effective_dir);

    assert(src_effective_dir != Port::IN);
    assert(sink_effective_dir != Port::OUT);

    // This is the name of the net. Non-export nets are named nodename_portname and exports are
    // just named exactly after the portname.
    std::string src_netname = src_is_export? 
        src->get_name() : src->get_parent()->get_node()->get_name() + "_" + src->get_name();

    // Try to find existing one
    Net* net = get_net(src_netname);
    if (!net)
    {
        // Not found? Create and bind to src.
        int src_width = src->eval_width();

        auto nettype = src_is_export? Net::EXPORT : Net::WIRE;
        net = new Net(nettype, src_netname);
        net->set_width(src_width);
        this->add_net(net);

        // Bind net to entire src.
        src->bind(net, src_width, 0, 0); 
    }

    // Now bind the net to the sink.
    // Specifically, bind [src_lsb+width-1, src_lsb] of the net to [sink_lsb+width-1, sink_lsb] of sink.
    sink->bind(net, width, sink_lsb, src_lsb);
}

void NodeHDLInfo::connect(Port* sink, const genie::Value& val, int sink_lsb)
{
    // Wrap the value in a ConstValue Bindable object and bind it to :
    //   sink[val.width + sink_lsb - 1 : sink_lsb]
    auto cv = new ConstValue(val);
    sink->bind(cv, sink_lsb, 0);

    // Keep track of it for memory cleanup later
    m_const_values.push_back(cv);
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

bool PortBinding::is_full_port_binding()
{
	bool result = 
		(m_port_lsb == 0) &&
		(m_width == m_parent->eval_width());

	return result;
}

bool PortBinding::is_full_target_binding()
{
	bool result =
		(m_target_lsb == 0) &&
		(m_width == m_target->get_width());

	return result;
}

//
// ConstValue
//

ConstValue::ConstValue()
{
}

ConstValue::ConstValue(const genie::Value& v)
	: m_value(v)
{
}

int ConstValue::get_width()
{
	return m_value.get_width();
}

std::string ConstValue::to_string()
{
	return m_value.to_string();
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

std::string Net::to_string()
{
	return m_name;
}

int Net::get_width()
{
	return m_width;
}

void Net::set_width(int width)
{
	m_width = width;
}





