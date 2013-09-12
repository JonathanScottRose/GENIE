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
// PortBinding
//

PortBinding::PortBinding()
	: m_parent(nullptr), m_port(nullptr), m_net(nullptr)
{
}

PortBinding::~PortBinding()
{
}

const std::string& PortBinding::get_name()
{
	return m_port->get_name();
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
	/*
	for (auto& i : module->ports())
	{
		Port* port = i.second;
		m_port_bindings[port->get_name()] = nullptr;
	}

	for (auto& i : module->ports())
	{
		Port* port = i.second;
		m_port_bindings[port->get_name()] = nullptr;
	}*/
}

Instance::~Instance()
{
	ct::Util::delete_all_2(m_port_bindings);
	ct::Util::delete_all_2(m_param_bindings);
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