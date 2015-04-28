#pragma once

#include "genie/common.h"
#include "genie/expressions.h"
#include "genie/networks.h"
#include "genie/value.h"
#include "genie/structure.h"

namespace genie
{
namespace vlog
{
	using Expression = expressions::Expression;
	using NameResolver = expressions::NameResolver;

	class NodeVlogInfo;
	class SystemVlogInfo;
	class Port;
	class PortBinding;

	class Bindable;
	class ConstValue;
	class Net;

	class Bindable
	{
	public:
		virtual int get_width() = 0;
		virtual std::string to_string() = 0;
	};

	class PortBinding
	{
	public:
		PortBinding(Port* parent, Bindable* target = nullptr);
		virtual ~PortBinding();

		PROP_GET(target, Bindable*, m_target);
		PROP_GET(parent, Port*, m_parent);
		PROP_GET_SET(port_lsb, int, m_port_lsb);
		PROP_GET_SET(target_lsb, int, m_target_lsb);
		PROP_GET_SET(width, int, m_width);

		bool is_full_target_binding();	// does this binding bind to the target's full width?
		bool is_full_port_binding();	// does this binding bind to the port's full width?

	protected:
		Port* m_parent;
		Bindable* m_target;
		int m_port_lsb;
		int m_target_lsb;
		int m_width;
	};

	class Port
	{
	public:
		typedef std::vector<PortBinding*> Bindings;

		enum Dir
		{
			IN,
			OUT,
			INOUT
		};

		static Dir rev_dir(Dir in);
		static Dir make_dir(genie::Dir, genie::SigRole::Sense);

		Port(const std::string& m_name);
		Port(const std::string& m_name, const Expression& width, Dir dir);
		Port(const Port&) = delete;
		virtual ~Port();

		PROP_GET_SET(name, const std::string&, m_name);
		PROP_GET_SET(dir, Dir, m_dir);
		PROP_GET_SET(parent, NodeVlogInfo*, m_parent);
		PROP_GET_SET(width, const Expression&, m_width);

		int eval_width();
		const Bindings& bindings() { return m_bindings; }
		bool is_bound();

		void bind(Bindable*, int port_lsb = 0, int target_lsb = 0);
		void bind(Bindable*, int width, int port_lsb, int target_lsb);

		Port* instantiate() const;

	protected:
		NodeVlogInfo* m_parent;
		Expression m_width;
		std::string m_name;
		Dir m_dir;
		Bindings m_bindings;
	};

	class ConstValue : public Bindable
	{
	public:
		ConstValue();
		ConstValue(const Value& v);
		
		PROP_GET_SET(value, const Value&, m_value);
		int get_width() override;
		std::string to_string() override;

	protected:
		Value m_value;
	};

	class Net : public Bindable
	{
	public:
		enum Type
		{
			WIRE,
			EXPORT
		};

		Net(Type type, const std::string& name);
		~Net();

		PROP_GET_SET(type, Type, m_type);
		PROP_GET(name, const std::string&, m_name);
		std::string to_string() override;
		int get_width() override;
		void set_width(int width);

	protected:
		std::string m_name;
		Type m_type;
		int m_width;
	};

	class NodeVlogInfo : public NodeHDLInfo
	{
	public:
		NodeVlogInfo(const std::string&);
		~NodeVlogInfo();

		PROP_GET_SET(module_name, const std::string&, m_mod_name);
		PROP_DICT_NOSET(Ports, port, Port);

		NodeHDLInfo* instantiate() override;
		Port* add_port(Port*);

	protected:
		std::string m_mod_name;
	};

	class SystemVlogInfo : public NodeVlogInfo
	{
	public:
		SystemVlogInfo(const std::string&);
		~SystemVlogInfo();

		PROP_DICT(Nets, net, Net);

		void connect(Port* src, Port* sink, int src_lsb, int sink_lsb, int width);
		void connect(Port* sink, const Value&, int sink_lsb);

	protected:
		List<ConstValue*> m_const_values;
	};

	void write_system(System*);
	void flow_process_system(System*);
	HDLBinding* export_binding(System*, genie::Port*, HDLBinding*);
}
}