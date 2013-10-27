#pragma once

#include "ct/common.h"
#include "ct/expressions.h"

using namespace ct::Expressions;

namespace Vlog
{
	class Module;
	class Port;
	class Parameter;
	class Instance;
	class PortBinding;
	class PortState;
	class ParamBinding;
	class Bindable;
	class ConstValue;
	class Net;
	class WireNet;
	class ExportNet;
	class SystemModule;
	class ModuleRegistry;
	class ContinuousAssignment;

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

		static Dir rev_dir(Dir in);

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
		typedef std::vector<PortBinding*> PortBindings;

		PortState();
		~PortState();

		PROP_GET_SET(parent, Instance*, m_parent);
		PROP_GET_SET(port, Port*, m_port);

		const std::string& get_name();
		int get_width();

		const PortBindings& bindings() { return m_bindings; }
		bool is_empty();
		bool has_multiple_bindings();

		void bind(Bindable*, int port_lo = 0, int target_lo = 0);
		void bind(Bindable*, int width, int port_lo, int target_lo);
	
	protected:
		PortBindings m_bindings;
		Instance* m_parent;
		Port* m_port;
	};

	class Bindable
	{
	public:
		enum Type
		{
			NET,
			CONST
		};

		Bindable(Type);
		virtual ~Bindable() = 0;
		virtual void dispose() = 0;

		PROP_GET(type, Type, m_type);
		virtual int get_width() = 0;
		// future: add string representation

	protected:
		Type m_type;
	};

	class PortBinding
	{
	public:
		PortBinding(PortState* parent, Bindable* target = nullptr);
		virtual ~PortBinding();

		PROP_GET_SET(target, Bindable*, m_target);
		PROP_GET_SET(port_lo, int, m_port_lo);
		PROP_GET_SET(target_lo, int, m_target_lo);
		PROP_GET_SET(width, int, m_width);
		PROP_GET_SET(parent, PortState*, m_parent);

		bool is_full_target_binding();	// does this binding bind to the target's full width?
		bool is_full_port_binding();	// does this binding bind to the port's full width?

	protected:
		PortState* m_parent;
		Bindable* m_target;
		int m_port_lo;
		int m_target_lo;
		int m_width;
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

	class ConstValue : public Bindable
	{
	public:
		ConstValue(int value, int width);
		void dispose();
		
		PROP_GET_SET(value, int, m_value);
		int get_width();

	protected:
		int m_width;
		int m_value;
	};

	class Net : public Bindable
	{
	public:
		enum Type
		{
			WIRE,
			EXPORT
		};

		Net(Type type);
		virtual ~Net();
		void dispose();

		PROP_GET_SET(type, Type, m_type);
		virtual const std::string& get_name() = 0;
		virtual int get_width() = 0;

	protected:
		Type m_type;
	};

	class WireNet : public Net
	{
	public:
		WireNet(const std::string& name, int width);
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
		
		void bind_port(const std::string& portname, Bindable*, int port_lo = 0, int target_lo = 0);
		void bind_port(const std::string& portname, Bindable*, int width, int port_lo, int target_lo);

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
		typedef std::vector<ContinuousAssignment*> ContAssigns;

		SystemModule();
		~SystemModule();

		PROP_DICT(Instances, instance, Instance);
		PROP_DICT(Nets, net, Net);

	protected:
		ContAssigns m_cont_assigns;
	};

	class ModuleRegistry
	{
	public:
		ModuleRegistry();
		~ModuleRegistry();

		PROP_DICT(Modules, module, Module);
	};

	class ContinuousAssignment
	{
	public:
		ContinuousAssignment(Bindable* src, Net* sink);
		~ContinuousAssignment();

		PROP_GET_SET(src, Bindable*, m_src);
		PROP_GET_SET(sink, Net*, m_sink);

	protected:
		Bindable* m_src;
		Net* m_sink;
	};

	int parse_constant(const std::string&);
}