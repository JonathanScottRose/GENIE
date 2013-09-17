#include "vlog.h"

using namespace Vlog;

//
// Port
//

Port::Port(const std::string& name, Module* parent)
	: m_parent(parent), m_name(name)
{
}

Port::~Port()
{
}

//
// Parameter
//

Parameter::Parameter(const std::string& name, Module* parent)
	: m_parent(parent), m_name(name), m_is_vec(false), m_def_val(0)
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
// PortBindingGroup
//

PortBindingGroup::PortBindingGroup()
	: m_parent(nullptr), m_port(nullptr)
{
}

PortBindingGroup::~PortBindingGroup()
{
	ct::Util::delete_all(m_bindings);
}

const std::string& PortBindingGroup::get_name()
{
	return m_port->get_name();
}

bool PortBindingGroup::is_simple()
{
	bool result = is_empty();
	if (!result)
	{
		result = m_bindings.front()->sole_binding();
	}

	return result;
}

bool PortBindingGroup::is_empty()
{
	return m_bindings.empty();
}

PortBinding* PortBindingGroup::get_sole_binding()
{
	if (is_empty())
		return nullptr;

	PortBinding* result = bindings().front();
	assert(result->sole_binding());
	return result;
}

PortBinding* PortBindingGroup::bind(Net* net, int port_lo, int net_lo)
{
	PortBinding* result = nullptr;

	Port* port = get_port();

	int width = port->get_width();
	int port_hi = port_lo + width - 1;

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
	result->set_width(width);

	m_bindings.push_front(result);
	m_bindings.sort([] (PortBinding* a, PortBinding* b)
	{
		return a->get_port_lo() > b->get_port_lo();
	});

	return result;
}

PortBinding* PortBindingGroup::bind(Net* net, int lo)
{
	return bind(net, lo, 0);
}

PortBinding* PortBindingGroup::bind(Net* net)
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
		(m_width == m_parent->get_port()->get_width());

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
	: m_parent(nullptr), m_value(0), m_param(nullptr)
{
}

ParamBinding::~ParamBinding()
{
}

const std::string& ParamBinding::get_name()
{
	return m_param->get_name();
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
		
		PortBindingGroup* group = new PortBindingGroup();
		group->set_parent(this);
		group->set_port(port);
		m_port_bindings[port->get_name()] = group;
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
}

Instance::~Instance()
{
	ct::Util::delete_all_2(m_port_bindings);
	ct::Util::delete_all_2(m_param_bindings);
}

PortBinding* Instance::get_sole_binding(const std::string& port)
{
	PortBindingGroup* grp = m_port_bindings[port];
	return grp->get_sole_binding();
}

PortBindingGroup* Instance::get_port_bindings(const std::string& name)
{
	return m_port_bindings[name];
}

PortBinding* Instance::bind_port(const std::string& pname, Net* net, int port_lo, int net_lo)
{
	PortBinding* result = nullptr;

	PortBindingGroup* grp = m_port_bindings[pname];
	return grp->bind(net, port_lo, net_lo);
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