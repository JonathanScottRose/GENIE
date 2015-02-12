#ifdef _WIN32
#include <regex>
using std::regex;
using std::smatch;
using std::regex_search;
#else
#include <boost/regex.hpp>
using boost::regex;
using boost::smatch;
using boost::regex_search;
#endif

#include "ct/common.h"
#include "vlog.h"

using namespace ct;
using namespace Vlog;

//
// Port
//

Port::Port(const std::string& name, Module* parent)
	: m_parent(parent), m_name(name)
{
}

Port::Port(const std::string& name, Module* parent, const Expression& width, Dir dir)
	: m_parent(parent), m_name(name), m_width(width), m_dir(dir)
{
}

Port::~Port()
{
}

void Port::set_width(int i)
{
	m_width = i;
}

void Port::set_width(const std::string& expr)
{
	m_width = expr;
}

int Port::get_width()
{
	return m_width.get_value();
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

//
// Parameter
//

Parameter::Parameter(const std::string& name, Module* parent)
	: m_parent(parent), m_name(name), m_def_val(0)
{
}

Parameter::~Parameter()
{
}

//
// Module
//

Module::Module()
{
}

Module::~Module()
{
	ct::Util::delete_all_2(m_ports);
	ct::Util::delete_all_2(m_params);
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
	ct::Util::delete_all(m_bindings);
}

const std::string& PortState::get_name()
{
	return m_port->get_name();
}

int PortState::get_width()
{
	return m_port->width().get_value(m_parent->get_param_resolver());
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
// ParamBinding
//

ParamBinding::ParamBinding()
	: m_parent(nullptr), m_param(nullptr)
{
}

ParamBinding::~ParamBinding()
{
}

const std::string& ParamBinding::get_name()
{
	return m_param->get_name();
}

void ParamBinding::set_value(int val)
{
	m_value = val;
}

void ParamBinding::set_hack_value(const std::string& val)
{
	m_value = Expression::build_hack_expression(val);
}

int ParamBinding::get_value()
{
	return m_value.get_value();
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
	return m_port->get_width();
}


//
// SystemModule
//

SystemModule::SystemModule()
{
	m_resolv = [this] (const std::string& name)
	{
		return get_localparam(name);
	};
}

SystemModule::~SystemModule()
{
	ct::Util::delete_all_2(m_instances);
	ct::Util::delete_all_2(m_nets);
	ct::Util::delete_all(m_cont_assigns);
}

Expression& SystemModule::get_localparam(const std::string& name)
{
	return m_localparams[name];
}

void SystemModule::add_localparam(const std::string& name, const Expression& v)
{
	m_localparams[name] = v;
}

//
// Instance
//

Instance::Instance(const std::string& name, Module* module)
	: m_name(name), m_module(module)
{
	for (auto& i : module->ports())
	{
		Port* port = i.second;
		
		PortState* st = new PortState();
		st->set_parent(this);
		st->set_port(port);
		m_port_states[port->get_name()] = st;
	}

	for (auto& i : module->params())
	{
		Parameter* param = i.second;

		ParamBinding* binding = new ParamBinding();
		binding->set_param(param);
		binding->set_parent(this);
		binding->set_value(param->get_def_val());
		m_param_bindings[binding->get_name()] = binding;
	}

	m_resolv = [this] (const std::string& name)
	{
		return get_param_binding(name)->value();
	};
}

Instance::~Instance()
{
	ct::Util::delete_all_2(m_port_states);
	ct::Util::delete_all_2(m_param_bindings);
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

ParamBinding* Instance::get_param_binding(const std::string& name)
{
	return m_param_bindings[name];
}

int Instance::get_param_value(const std::string& name)
{
	return get_param_binding(name)->get_value();
}

void Instance::set_param_value(const std::string& name, int val)
{
	get_param_binding(name)->set_value(val);
}

void Instance::set_param_value(const std::string& name, const std::string& val)
{
	get_param_binding(name)->set_hack_value(val);
}

//
// ModuleRegistry
//

ModuleRegistry::ModuleRegistry()
{
}

ModuleRegistry::~ModuleRegistry()
{
	ct::Util::delete_all_2(m_modules);
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
// Globals
//

int Vlog::parse_constant(const std::string& val)
{
	int base = 10;

	regex regex("\\s*(\\d+)'([dbho])([0-9abcdefABCDEF]+)");
	smatch mr;

	regex_match(val, mr, regex);
	int bits = std::stoi(mr[1]);
	char radix = mr[2].str().at(0);
	switch (radix)
	{
	case 'd': base = 10; break;
	case 'h': base = 16; break;
	case 'o': base = 8; break;
	case 'b': base = 2; break;
	default: assert(false);
	}
	
	return std::stoi(mr[3], 0, base);
}
