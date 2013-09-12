#pragma once

#include "ct/common.h"

namespace Vlog
{

class Module;
class Port;
class Parameter;
class Instance;
class PortBinding;
class ParamBinding;
class Net;
class WireNet;
class ExportNet;
class SystemModule;
class ModuleRegistry;

class Port
{
public:
	enum Dir
	{
		IN,
		OUT,
		INOUT
	};

	Port(const std::string& m_name, Module* parent);
	virtual ~Port();

	PROP_GET_SET(is_vec, bool, m_is_vec);
	PROP_GET_SET(width, int, m_width);
	PROP_GET_SET(name, const std::string&, m_name);
	PROP_GET_SET(dir, Dir, m_dir);
	PROP_GET_SET(parent, Module*, m_parent);

protected:
	Module* m_parent;
	bool m_is_vec;
	int m_width;
	std::string m_name;
	Dir m_dir;
};

class Parameter
{
public:
	Parameter(const std::string& name, Module* parent);
	virtual ~Parameter();

	PROP_GET_SET(name, const std::string&, m_name);
	PROP_GET_SET(width, int, m_width);
	PROP_GET_SET(def_val, int, m_def_val);
	PROP_GET_SET(is_vec, bool, m_is_vec);
	PROP_GET_SET(parent, Module*, m_parent);

protected:
	Module* m_parent;
	std::string m_name;
	int m_width;
	bool m_is_vec;
	int m_def_val;
};

class Module
{
public:
	Module();
	virtual ~Module();

	PROP_GET_SET(name, const std::string&, m_name);
	PROP_DICT(Params, param, Parameter);
	PROP_DICT(Ports, port, Port);

protected:
	std::string m_name;
};

class PortBinding
{
public:
	PortBinding();
	~PortBinding();

	PROP_GET_SET(parent, Instance*, m_parent);
	PROP_GET_SET(port, Port*, m_port);
	PROP_GET_SET(net, Net*, m_net);

	const std::string& get_name();

protected:
	Instance* m_parent;
	Port* m_port;
	Net* m_net;
};

class ParamBinding
{
public:
	ParamBinding();
	~ParamBinding();

	PROP_GET_SET(value, int, m_value);
	PROP_GET_SET(parent, Instance*, m_parent);
	PROP_GET_SET(param, Parameter*, m_param);

	const std::string& get_name();

protected:
	int m_value;
	Instance* m_parent;
	Parameter* m_param;
};

class Net
{
public:
	enum Type
	{
		WIRE,
		EXPORT
	};

	Net(Type type);
	virtual ~Net();

	PROP_GET_SET(type, Type, m_type);
	virtual const std::string& get_name() = 0;

protected:
	Type m_type;
};

class WireNet : public Net
{
public:
	WireNet();
	virtual ~WireNet();

	PROP_GET_SET(width, int, m_width);

	const std::string& get_name();
	void set_name(const std::string& name);

protected:
	std::string m_name;
	int m_width;
};

class ExportNet : public Net
{
public:
	ExportNet(Port* port);
	virtual ~ExportNet();

	PROP_GET_SET(port, Port*, m_port);
	
	const std::string& get_name();

protected:
	Port* m_port;
};

class Instance
{
public:
	Instance(const std::string& name, Module* module);
	virtual ~Instance();
	
	PROP_GET_SET(name, const std::string&, m_name);
	PROP_GET_SET(module, Module*, m_module);
	PROP_DICT(PortBindings, port_binding, PortBinding);
	PROP_DICT(ParamBindings, param_binding, ParamBinding);

protected:
	std::string m_name;
	Module* m_module;

};

class SystemModule : public Module
{
public:
	SystemModule();
	~SystemModule();

	PROP_DICT(Instances, instance, Instance);
	PROP_DICT(Nets, net, Net);
};

class ModuleRegistry
{
public:
	ModuleRegistry();
	~ModuleRegistry();

	PROP_DICT(Modules, module, Module);
};


}