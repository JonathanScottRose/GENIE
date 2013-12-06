#pragma once

#include "common.h"

namespace ct
{
namespace Core
{
	class Node;
	class Port;
	class Conn;
	class Protocol;
	class System;
	class Registry;
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
		int const_value;
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
		typedef std::unordered_map<std::string, Port*> Ports;
		typedef unsigned int Type;

		Node();
		Node(Type type);
		virtual ~Node();

		PROP_GET_SET(name, const std::string&, m_name);
		PROP_GET_SET(type, Type, m_type);
		PROP_GET_SET(parent, System*, m_parent);

		const Ports& ports() { return m_ports; }
		Port* get_port(const std::string& name) { return m_ports[name]; }
		void add_port(Port* port);
		void remove_port(Port* port);
		void remove_port(const std::string& name);
	
	protected:
		Type m_type;
		std::string m_name;
		Ports m_ports;
		System* m_parent;
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
		DataPort(Node* node, Dir dir);
		~DataPort();

		PROP_GET_SET(clock, ClockResetPort*, m_clock);

	protected:
		ClockResetPort* m_clock;
	};

	class Conn
	{
	public:
		typedef std::list<Port*> Sinks;

		Conn();
		Conn(Port* src, Port* sink);

		PROP_GET_SET(source, Port*, m_source);

		void set_sink0(Port* sink);
		Port* get_sink0();

		const Sinks& get_sinks() { return m_sinks; }
		void add_sink(Port* sink);
		void remove_sink(Port* sink);

	protected:
		Port* m_source;
		Sinks m_sinks;
	};

	class System
	{
	public:
		typedef std::vector<Conn*> ConnVec;
		typedef std::unordered_map<std::string, Node*> Nodes;

		System(const std::string& name);
		~System();

		PROP_GET(name, const std::string&, m_name);
		
		const Nodes& nodes() { return m_nodes; }
		Node* get_node(const std::string& name);
		void add_node(Node* node);
		bool has_node(const std::string& name);
		void remove_node(Node* node);

		const ConnVec& conns() { return m_conns; }
		void add_conn(Conn* conn);
		void remove_conn(Conn* conn);

		void connect_ports(Port* src, Port* dest);
		void disconnect_ports(Port* src, Port* dest);

		class ExportNode* get_export_node();
		ClockResetPort* get_a_reset_port();

		void dump_graph();

	protected:
		ExportNode* m_export;
		std::string m_name;
		ConnVec m_conns;
		Nodes m_nodes;
	};

	class Registry
	{
	public:
		struct NodeTypeInfo
		{
			std::string name;
		};

		typedef std::vector<NodeTypeInfo*> NodeTypes;

		Registry();
		~Registry();

		Node::Type register_nodetype(const std::string& name);
		Node::Type get_nodetype(const std::string& name);
		const NodeTypeInfo* get_nodetype_info(Node::Type type);
		const NodeTypeInfo* get_nodetype_info(const std::string& name);
		const std::string& get_nodetype_name(Node::Type type);

	protected:
		NodeTypes m_node_types;
	};
}
}