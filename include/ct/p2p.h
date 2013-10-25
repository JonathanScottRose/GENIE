#pragma once

#include "common.h"
#include "spec.h"

namespace ct
{
namespace P2P
{
	class Node;
	class Port;
	class Flow;
	class Conn;
	class Protocol;
	class System;
	struct Field;

	struct Field
	{
		enum Sense
		{
			FWD,
			REV
		};

		Field();
		Field(const std::string&, int, Sense);

		std::string name;
		int width;
		Sense sense;
		bool is_const;
		int const_val;
	};

	class Protocol
	{
	public:
		typedef std::list<Field*> Fields;

		Protocol();
		Protocol(const Protocol&);
		Protocol& operator= (const Protocol&);
		~Protocol();
		
		const Fields& fields() const { return m_fields; }

		void add_field(Field* f);
		Field* get_field(const std::string& name) const;
		bool has_field(const std::string& name) const;
		void remove_field(Field* f);
		void delete_field(const std::string& name);

	protected:
		Fields m_fields;
	};

	class Node : public HasImplAspects
	{
	public:
		typedef std::unordered_map<std::string, Port*> PortMap;

		enum Type
		{
			INSTANCE,
			SPLIT,
			MERGE,
			FLOW_CONV,
			EXPORT
		};

		Node(Type type);
		virtual ~Node();

		PROP_GET_SET(name, const std::string&, m_name);
		PROP_GET_SET(type, Type, m_type);
		PROP_GET_SET(parent, Node*, m_parent);

		const PortMap& ports() { return m_ports; }
		Port* get_port(const std::string& name) { return m_ports[name]; }
		void add_port(Port* port);
		void remove_port(Port* port);
		void remove_port(const std::string& name);
	
	protected:
		Type m_type;
		std::string m_name;
		PortMap m_ports;
		Node* m_parent;
	};

	class Port : public HasImplAspects
	{
	public:
		enum Type
		{		
			CLOCK,
			RESET,
			DATA,
			CONDUIT
		};

		enum Dir
		{
			IN,
			OUT,
			MIXED
		};

		Port(Type type, Dir dir, Node* node);
		virtual ~Port();

		PROP_GET_SET(name, const std::string&, m_name);
		PROP_GET_SET(type, Type, m_type);
		PROP_GET_SET(dir, Dir, m_dir);
		PROP_GET_SET(parent, Node*, m_parent);
		PROP_GET_SET(conn, Conn*, m_conn);
		PROP_GET_SET(proto, const Protocol&, m_proto);

		static Dir rev_dir(Dir dir);

	protected:
		Type m_type;
		Dir m_dir;
		std::string m_name;
		Node* m_parent;	
		Conn* m_conn;
		Protocol m_proto;
	};

	class ClockResetPort : public Port
	{
	public:
		ClockResetPort(Type type, Dir dir, Node* node);
		~ClockResetPort();
	};

	class ConduitPort : public Port
	{
	public:
		ConduitPort(Node* node);
		~ConduitPort();
	};

	class DataPort : public Port
	{
	public:
		typedef std::vector<Flow*> Flows;

		DataPort(Node* node, Dir dir);
		~DataPort();

		PROP_GET_SET(clock, ClockResetPort*, m_clock);
		

		const Flows& flows() { return m_flows; }
		void add_flow(Flow* f);
		void remove_flow(Flow* f);
		bool has_flow(Flow* f);
		void clear_flows();
		void add_flows(const Flows& f);

	protected:
		ClockResetPort* m_clock;
		Flows m_flows;
	};

	class Conn
	{
	public:
		typedef std::forward_list<Port*> Sinks;

		Conn();
		Conn(Port* src, Port* sink);

		PROP_GET_SET(source, Port*, m_source);

		void set_sink(Port* sink);
		Port* get_sink();

		const Sinks& get_sinks() { return m_sinks; }
		void add_sink(Port* sink);
		void remove_sink(Port* sink);

	protected:
		Port* m_source;
		Sinks m_sinks;
	};

	class FlowTarget
	{
	public:
		FlowTarget();
		FlowTarget(DataPort*, const Spec::LinkTarget&);
		Spec::Linkpoint* get_linkpoint() const;

		DataPort* port;
		Spec::LinkTarget lt;
	};
	
	class Flow
	{
	public:
		typedef std::forward_list<FlowTarget*> Sinks;

		Flow();
		~Flow();

		PROP_GET_SET(id, int, m_id);
		
		void set_src(DataPort* port, const Spec::LinkTarget& lt);
		FlowTarget* get_src();
		
		FlowTarget* get_sink();
		FlowTarget* get_sink(DataPort* port);
		void set_sink(DataPort* port, const Spec::LinkTarget& lt);
		const Sinks& sinks() { return m_sinks; }
		void add_sink(DataPort* sink, const Spec::LinkTarget& lt);
		void remove_sink(FlowTarget* ft);
		bool has_sink(DataPort* port);

	protected:
		FlowTarget* m_src;
		Sinks m_sinks;
		int m_id;
	};

	class System
	{
	public:
		typedef std::vector<Conn*> ConnVec;
		typedef std::unordered_map<int, Flow*> Flows;

		System(const std::string& name);
		~System();

		PROP_GET_SET(spec, Spec::System*, m_sys_spec);
		PROP_GET(name, const std::string&, m_name);
		PROP_DICT(Nodes, node, Node);

		const ConnVec& conns() { return m_conns; }
		void add_conn(Conn* conn);
		void remove_conn(Conn* conn);

		const Flows& flows() { return m_flows; }
		Flow* get_flow(int id) { return m_flows[id]; }
		void add_flow(Flow* flow);

		void connect_ports(DataPort* src, DataPort* dest);
		void disconnect_ports(DataPort* src, DataPort* dest);

		class ExportNode* get_export_node();
		ClockResetPort* get_a_reset_port();

		void dump_graph();

	protected:
		std::string m_name;
		Spec::System* m_sys_spec;
		Flows m_flows;
		ConnVec m_conns;
	};
}
}