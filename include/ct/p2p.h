#pragma once

#include "ct/common.h"
#include "ct/spec.h"
#include "ct/protocol.h"

namespace ct
{
namespace P2P
{
	class Node;
	class Port;
	class Flow;
	class Conn;
	class System;

	class Node : public HasImplAspects
	{
	public:
		typedef std::unordered_map<std::string, Port*> PortMap;
		typedef std::vector<Port*> PortList;

		enum Type
		{
			INSTANCE,
			SPLIT,
			MERGE,
			FLOW_CONV,
			EXPORT,
			CLOCK_CROSS,
			REG_STAGE
		};

		Node(Type type);
		virtual ~Node();

		PROP_GET_SET(name, const std::string&, m_name);
		PROP_GET_SET(type, Type, m_type);
		PROP_GET_SET(parent, System*, m_parent);

		virtual void configure_1() {}
		virtual void configure_2() {}
		virtual PortList trace(Port* in, Flow* f) { return PortList(); }
		virtual Port* rtrace(Port* out, Flow* f) { return nullptr; }
		virtual void carry_fields(const FieldSet& set) { }

		const PortMap& ports() { return m_ports; }
		Port* get_port(const std::string& name) { return m_ports[name]; }
		void add_port(Port* port);
		void remove_port(Port* port);
		void remove_port(const std::string& name);
	
	protected:
		Type m_type;
		std::string m_name;
		PortMap m_ports;
		System* m_parent;
	};

	class Port : public HasImplAspects
	{
	public:
		typedef std::vector<Flow*> Flows;

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
			OUT
		};

		Port(Type type, Dir dir, Node* node);
		virtual ~Port();

		PROP_GET_SET(name, const std::string&, m_name);
		PROP_GET_SET(type, Type, m_type);
		PROP_GET_SET(dir, Dir, m_dir);
		PROP_GET_SET(parent, Node*, m_parent);
		PROP_GET_SET(conn, Conn*, m_conn);
		
		void set_proto(const Protocol& proto) { m_proto = proto; }
		Protocol& get_proto() { return m_proto; }

		const Flows& flows() { return m_flows; }
		void add_flow(Flow* f);
		void remove_flow(Flow* f);
		bool has_flow(Flow* f);
		void clear_flows();
		void add_flows(const Flows& f);

		const Spec::Links& links() const { return m_links; };
		void add_link(Spec::Link* link) { m_links.push_back(link); }
		void add_links(const Spec::Links&);
		bool has_link(Spec::Link*) const;

		Port* get_driver();
		Port* get_first_connected_port();
		bool is_connected();

		static Dir rev_dir(Dir dir);
		static Port* from_interface(Spec::Interface*, const Expressions::NameResolver&);

	protected:
		Type m_type;
		Dir m_dir;
		std::string m_name;
		Node* m_parent;	
		Conn* m_conn;
		Protocol m_proto;
		Flows m_flows;
		Spec::Links m_links;
	};

	class ClockResetPort : public Port
	{
	public:
		ClockResetPort(Type type, Dir dir, Node* node = nullptr);
	};

	class ConduitPort : public Port
	{
	public:
		ConduitPort(Dir dir, Node* node = nullptr);
	};

	class DataPort : public Port
	{
	public:
		DataPort(Dir dir, Node* node = nullptr);

		PROP_GET_SET(clock, ClockResetPort*, m_clock);

	protected:
		ClockResetPort* m_clock;
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
		FlowTarget(Port*, const Spec::LinkTarget&);
		Spec::Linkpoint* get_linkpoint() const;

		bool operator== (const FlowTarget& other) const;

		Port* port;
		Spec::LinkTarget lt;
	};
	
	class Flow
	{
	public:
		typedef std::forward_list<FlowTarget*> Sinks;
		typedef std::forward_list<Spec::Link*> Links;

		Flow();
		~Flow();

		PROP_GET_SET(id, int, m_id);
		
		void set_src(Port* port, const Spec::LinkTarget& lt);
		FlowTarget* get_src();
		
		FlowTarget* get_sink();
		FlowTarget* get_sink(Port* port);
		FlowTarget* get_sink0();
		void set_sink(Port* port, const Spec::LinkTarget& lt);
		const Sinks& sinks() { return m_sinks; }
		void add_sink(Port* sink, const Spec::LinkTarget& lt);
		void remove_sink(FlowTarget* ft);
		bool has_sink(Port* port);

		const Links& links() const { return m_links; }
		void add_link(Spec::Link* l) { m_links.push_front(l); }

	protected:
		FlowTarget* m_src;
		Sinks m_sinks;
		Links m_links;
		int m_id;
	};

	class System
	{
	public:
		typedef std::vector<Conn*> ConnVec;
		typedef std::unordered_map<int, Flow*> Flows;
		typedef std::unordered_map<std::string, Node*> Nodes;

		System(const std::string& name);
		~System();

		PROP_GET_SET(spec, Spec::System*, m_sys_spec);
		PROP_GET(name, const std::string&, m_name);
		
		const Nodes& nodes() { return m_nodes; }
		Node* get_node(const std::string& name);
		void add_node(Node* node);
		bool has_node(const std::string& name);

		const ConnVec& conns() { return m_conns; }
		void add_conn(Conn* conn);
		void remove_conn(Conn* conn);

		const Flows& flows() { return m_flows; }
		Flow* get_flow(int id) { return m_flows[id]; }
		void add_flow(Flow* flow);
		int get_global_flow_id_width();

		Conn* connect_ports(Port* src, Port* dest);
		void disconnect_ports(Port* src, Port* dest);
		void splice_conn(Conn* conn, Port* new_in, Port* new_out);
		void connect_clock_src(DataPort* target_port, ClockResetPort* clock_src);

		class ExportNode* get_export_node();
		ClockResetPort* get_a_reset_port();

		void dump_graph();

	protected:
		std::string m_name;
		Spec::System* m_sys_spec;
		Flows m_flows;
		ConnVec m_conns;
		Nodes m_nodes;
	};
}
}