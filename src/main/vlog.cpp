#include <regex>
#include "vlog.h"

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
	return m_width.get_const_value();
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

bool PortState::is_simple()
{
	bool result = is_empty();
	if (!result)
	{
		result = m_bindings.front()->sole_binding();
	}

	return result;
}

bool PortState::is_empty()
{
	return m_bindings.empty();
}

PortBinding* PortState::get_sole_binding()
{
	if (is_empty())
		return nullptr;

	PortBinding* result = bindings().front();
	assert(result->sole_binding());
	return result;
}

PortBinding* PortState::bind(Net* net, int port_lo, int net_lo)
{
	PortBinding* result = nullptr;

	Port* port = get_port();

	int port_width = get_width();
	int net_width = net->get_width();
	int port_hi = port_lo + net_width - 1;

	for (auto& i : bindings())
	{
		assert(i->get_port_lo() > port_hi ||
			(i->get_port_lo() + i->get_width() - 1 < port_lo));
	}

	result = new PortBinding();
	result->set_net(net);
	result->set_net_lo(net_lo);
	result->set_parent(this);
	result->set_port_lo(port_lo);
	result->set_width(net_width);

	m_bindings.push_front(result);
	m_bindings.sort([] (PortBinding* a, PortBinding* b)
	{
		return a->get_port_lo() > b->get_port_lo();
	});

	return result;
}

PortBinding* PortState::bind(Net* net, int lo)
{
	return bind(net, lo, 0);
}

PortBinding* PortState::bind(Net* net)
{
	return bind(net, 0);
}

//
// PortBinding
//

PortBinding::PortBinding()
	: m_net(nullptr), m_parent(nullptr)
{
}

PortBinding::~PortBinding()
{
}

bool PortBinding::sole_binding()
{
	bool result = 
		(m_port_lo == 0) &&
		(m_width == m_parent->get_width());

	return result;
}

bool PortBinding::target_simple()
{
	return m_net->get_width() == m_width;
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

int ParamBinding::get_value()
{
	return m_value.get_const_value();
}


//
// Net
//

Net::Net(Type type)
	: m_type(type)
{
}

Net::~Net()
{
}

//
// WireNet
//

WireNet::WireNet()
	: Net(Net::WIRE), m_width(0)
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
}

SystemModule::~SystemModule()
{
	ct::Util::delete_all_2(m_instances);
	ct::Util::delete_all_2(m_nets);
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
		return &(get_param_binding(name)->value());
	};
}

Instance::~Instance()
{
	ct::Util::delete_all_2(m_port_states);
	ct::Util::delete_all_2(m_param_bindings);
}

PortBinding* Instance::get_sole_binding(const std::string& port)
{
	PortState* grp = m_port_states[port];
	return grp->get_sole_binding();
}

PortState* Instance::get_port_state(const std::string& name)
{
	return m_port_states[name];
}

PortBinding* Instance::bind_port(const std::string& pname, Net* net, int port_lo, int net_lo)
{
	PortBinding* result = nullptr;

	PortState* st = m_port_states[pname];
	return st->bind(net, port_lo, net_lo);
}

PortBinding* Instance::bind_port(const std::string& port, Net* net, int lo)
{
	return bind_port(port, net, lo, 0);
}

PortBinding* Instance::bind_port(const std::string& port, Net* net)
{
	return bind_port(port, net, 0);
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
// Globals
//

int Vlog::parse_constant(const std::string& val)
{
	int base = 10;

	std::regex regex("\\s*(\\d+)'([dbho])([0-9abcdefABCDEF]+)");
	std::smatch mr;

	std::regex_match(val, mr, regex);
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
