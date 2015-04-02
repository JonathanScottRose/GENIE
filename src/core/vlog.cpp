#include "genie/common.h"
#include "genie/vlog.h"
#include "genie/structure.h"

using namespace genie::vlog;

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

Port::~Port()
{
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

//
// Module
//

Module::Module(const std::string& name)
	: m_name(name)
{
}

Module::~Module()
{
	util::delete_all_2(m_ports);
}

Port* Module::add_port(Port* p)
{
	assert(!util::exists_2(m_ports, p->get_name()));
	m_ports[p->get_name()] = p;
	p->set_parent(this);
	return p;
}

//
// PortState
//

PortState::PortState()
	: m_parent(nullptr), m_port(nullptr)
{
	
}

PortState::~PortState()
{
	util::delete_all(m_bindings);
}

const std::string& PortState::get_name()
{
	return m_port->get_name();
}

int PortState::get_width()
{
	return m_port->get_width().get_value(m_parent->get_node()->get_exp_resolver());
}

bool PortState::has_multiple_bindings()
{
	bool result = is_empty();
	if (!result)
	{
		result = !m_bindings.front()->is_full_port_binding();
	}

	return result;
}

bool PortState::is_empty()
{
	return m_bindings.empty();
}

void PortState::bind(Bindable* target, int width, int port_lo, int targ_lo)
{
	// Check if this binding will overlap with existing bindings (at the port binding site)
	for (auto& b : m_bindings)
	{
		assert(port_lo + width <= b->get_port_lo() ||
			port_lo >= b->get_port_lo() + b->get_width());
	}

	Port* port = get_port();

	int port_width = get_width();
	int targ_width = target->get_width();
	int port_hi = port_lo + width - 1;
	int targ_hi = targ_lo + width - 1;

	
	assert(port_hi < port_lo + port_width);
	assert(targ_hi < targ_lo + targ_width);

	// Create the binding and add it to our list of bindings
	PortBinding* binding = new PortBinding(this, target);
	binding->set_port_lo(port_lo);
	binding->set_target_lo(targ_lo);
	binding->set_width(width);
	m_bindings.push_back(binding);
}

void PortState::bind(Bindable* target, int port_lo, int targ_lo)
{
	// This version of bind automatically calculates the maximum possible width that can be bound
	int width = std::min(
		target->get_width() - targ_lo,
		this->get_width() - port_lo
	);

	bind(target, width, port_lo, targ_lo);
}


//
// PortBinding
//

PortBinding::PortBinding(PortState* parent, Bindable* target)
	: m_parent(parent), m_target(target)
{
}

PortBinding::~PortBinding()
{
	// Some targets like nets are owned by the netlist and will be cleaned up when the netlist dies,
	// but some like constant value drivers are not kept track by the netlist and are effectively owned
	// by the bindings. This handles destruction in a polymorphic way.
	if (m_target) m_target->dispose();
}

bool PortBinding::is_full_port_binding()
{
	bool result = 
		(m_port_lo == 0) &&
		(m_width == m_parent->get_width());

	return result;
}

bool PortBinding::is_full_target_binding()
{
	bool result =
		(m_target_lo == 0) &&
		(m_width == m_target->get_width());

	return result;
}

//
// ConstValue
//

ConstValue::ConstValue(int value, int width)
	: Bindable(CONST), m_value(value), m_width(width)
{
}

int ConstValue::get_width()
{
	return m_width;
}

void ConstValue::dispose()
{
	// consts are bound to only one thing, so that thing effectively owns the memory
	// for the const and the const should die when it gets its single call to dispose()
	delete this;
}

//
// Net
//

Net::Net(Type type)
	: Bindable(NET), m_type(type)
{
}

Net::~Net()
{
}

void Net::dispose()
{
	// do nothing, since nets are owned and deleted by the SystemModule netlist
}

//
// WireNet
//

WireNet::WireNet(const std::string& name, int width)
	: Net(Net::WIRE), m_width(width), m_name(name)
{
}

WireNet::~WireNet()
{
}

const std::string& WireNet::get_name()
{
	return m_name;
}

void WireNet::set_name(const std::string& name)
{
	m_name = name;
}

int WireNet::get_width()
{
	return m_width;
}

void WireNet::set_width(int width)
{
	m_width = width;
}

//
// ExportNet
//

ExportNet::ExportNet(Port* port)
	: Net(Net::EXPORT), m_port(port)
{
}

ExportNet::~ExportNet()
{
}

const std::string& ExportNet::get_name()
{
	assert(m_port != nullptr);
	return m_port->get_name();
}

int ExportNet::get_width()
{
	return m_port->get_width().get_value();
}


//
// SystemModule
//

SystemModule::SystemModule(const std::string& name)
	: Module(name), m_sysnode(nullptr)
{
}

SystemModule::~SystemModule()
{
	util::delete_all_2(m_instances);
	util::delete_all_2(m_nets);
}

//
// Instance
//

Instance::Instance(const std::string& name, Module* module)
	: m_name(name), m_module(module), m_node(nullptr)
{
	for (auto& i : module->ports())
	{
		Port* port = i.second;
		
		PortState* st = new PortState();
		st->set_parent(this);
		st->set_port(port);
		m_port_states[port->get_name()] = st;
	}
}

Instance::~Instance()
{
	util::delete_all_2(m_port_states);
}

PortState* Instance::get_port_state(const std::string& name)
{
	return m_port_states[name];
}

void Instance::bind_port(const std::string& portname, Bindable* targ, int port_lo, int targ_lo)
{
	PortState* ps = get_port_state(portname);
	assert(ps);
	ps->bind(targ, port_lo, targ_lo);
}

void Instance::bind_port(const std::string& portname, Bindable* targ, int width, int port_lo, int targ_lo)
{
	PortState* ps = get_port_state(portname);
	assert(ps);
	ps->bind(targ, width, port_lo, targ_lo);
}

//
// Bindable
//

Bindable::Bindable(Type type)
	: m_type(type)
{
}

Bindable::~Bindable()
{
}

//
// ContinuousAssignment
//

ContinuousAssignment::ContinuousAssignment(Bindable* src, Net* sink)
: m_src(src), m_sink(sink)
{
}

ContinuousAssignment::~ContinuousAssignment()
{
}

//
// Module Registry
//

namespace
{
	class
	{
	public:
		PROP_DICT(Modules, module, Module);
	} s_module_registry;
}

//
// Globals
//

void genie::vlog::register_module(Module* mod)
{
	if (s_module_registry.has_module(mod->get_name()))
		throw Exception("duplicate Verilog module " + mod->get_name());

	s_module_registry.add_module(mod);	
}

Module* genie::vlog::get_module(const std::string& name)
{
	return s_module_registry.get_module(name);
}