#pragma once

#include "ct/common.h"
#include "ct/expressions.h"

using ct::Expressions::Expression;
using ct::Expressions::NameResolver;

namespace Vlog
{

class Module;
class Port;
class Parameter;
class Instance;
class PortBinding;
class PortState;
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
	Port(const std::string& m_name, Module* parent, const Expression& width, Dir dir);
	virtual ~Port();

	PROP_GET_SET(name, const std::string&, m_name);
	PROP_GET_SET(dir, Dir, m_dir);
	PROP_GET_SET(parent, Module*, m_parent);

	Expression& width() { return m_width; }
	void set_width(int i);
	void set_width(const std::string& expr);
	int get_width();

protected:
	Module* m_parent;
	Expression m_width;
	std::string m_name;
	Dir m_dir;
};

class Parameter
{
public:
	Parameter(const std::string& name, Module* parent);
	virtual ~Parameter();

	PROP_GET_SET(name, const std::string&, m_name);
	PROP_GET_SET(def_val, int, m_def_val);
	PROP_GET_SET(parent, Module*, m_parent);

protected:
	Module* m_parent;
	std::string m_name;
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

class PortState
{
public:
	typedef std::forward_list<PortBinding*> PortBindings;

	PortState();
	~PortState();

	PROP_GET_SET(parent, Instance*, m_parent);
	PROP_GET_SET(port, Port*, m_port);

	const std::string& get_name();
	int get_width();

	const PortBindings& bindings() { return m_bindings; }
	bool is_empty();
	bool is_simple();

	PortBinding* get_sole_binding();
	PortBinding* bind(Net* net);
	PortBinding* bind(Net* net, int lo);
	PortBinding* bind(Net* net, int port_lo, int net_lo);
	PortBinding* bind_const(int val, int val_width, int port_lo = 0);
	
protected:
	PortBindings m_bindings;
	Instance* m_parent;
	Port* m_port;
};

class PortBinding
{
public:
	PortBinding();
	~PortBinding();

	PROP_GET_SET(net, Net*, m_net);
	PROP_GET_SET(port_lo, int, m_port_lo);
	PROP_GET_SET(net_lo, int, m_net_lo);
	PROP_GET_SET(width, int, m_width);
	PROP_GET_SET(parent, PortState*, m_parent);
	PROP_GET_SET(is_const, bool, m_is_const);
	PROP_GET_SET(const_val, int, m_const_val);

	bool sole_binding();
	bool target_simple();

protected:
	Net* m_net;
	PortState* m_parent;
	int m_port_lo;
	int m_net_lo;
	int m_width;
	bool m_is_const;
	int m_const_val;
};

class ParamBinding
{
public:
	ParamBinding();
	~ParamBinding();

	PROP_GET_SET(parent, Instance*, m_parent);
	PROP_GET_SET(param, Parameter*, m_param);

	const std::string& get_name();
	Expression& value() { return m_value; }
	void set_value(int val);
	int get_value();

protected:
	Expression m_value;
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
	virtual int get_width() = 0;

protected:
	Type m_type;
};

class WireNet : public Net
{
public:
	WireNet();
	virtual ~WireNet();

	const std::string& get_name();
	void set_name(const std::string& name);
	int get_width();
	void set_width(int width);

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
	int get_width();

protected:
	Port* m_port;
};

class Instance
{
public:
	typedef std::unordered_map<std::string, PortState*> PortStates;
	typedef std::unordered_map<std::string, ParamBinding*> ParamBindings;

	Instance(const std::string& name, Module* module);
	virtual ~Instance();
	
	PROP_GET_SET(name, const std::string&, m_name);
	PROP_GET_SET(module, Module*, m_module);
	
	const PortStates& port_states() { return m_port_states; }
	PortState* get_port_state(const std::string& name);
	PortBinding* get_sole_binding(const std::string& port);
	PortBinding* bind_port(const std::string& port, Net* net);
	PortBinding* bind_port(const std::string& port, Net* net, int lo);
	PortBinding* bind_port(const std::string& port, Net* net, int port_lo, int net_lo);
	PortBinding* bind_const(const std::string& port, int val, int val_width, int port_lo = 0);

	const ParamBindings& param_bindings() { return m_param_bindings; }
	ParamBinding* get_param_binding(const std::string& name);
	int get_param_value(const std::string& name);
	void set_param_value(const std::string& name, int val);

	const NameResolver& get_param_resolver() { return m_resolv; }

protected:
	NameResolver m_resolv;
	std::string m_name;
	Module* m_module;
	PortStates m_port_states;
	ParamBindings m_param_bindings;
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

int parse_constant(const std::string&);

}