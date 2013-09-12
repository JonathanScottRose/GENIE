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

	class Protocol
	{
	public:
		struct Field
		{
			enum Sense
			{
				FWD,
				REV
			};

			Field();
			Field(const std::string&, int, Sense, Spec::Signal* sigdef = nullptr);

			std::string name;
			int width;
			Spec::Signal* sigdef;
			Sense sense;
		};

		typedef std::forward_list<Field*> Fields;

		Protocol();
		Protocol(const Protocol&);
		Protocol& operator= (const Protocol&);
		~Protocol();
		
		const Fields& fields() const { return m_fields; }

		void add_field(Field* f);
		Field* get_field(const std::string& name) const;
		bool has_field(const std::string& name);

	protected:
		Fields m_fields;
	};

	class Node
	{
	public:
		typedef std::unordered_map<std::string, Port*> PortMap;

		enum Type
		{
			SYSTEM,
			INSTANCE,
			SPLIT,
			MERGE
		};

		Node(Type type);
		virtual ~Node();

		PROP_GET_SET(name, const std::string&, m_name);
		PROP_GET_SET(type, Type, m_type);
		PROP_GET_SET(parent, Node*, m_parent);

		const PortMap& ports() { return m_ports; }
		Port* get_port(const std::string& name) { return m_ports[name]; }
		void add_port(Port* port);		
	
	protected:
		Type m_type;
		std::string m_name;
		PortMap m_ports;
		Node* m_parent;
	};

	#undef IN
	#undef OUT

	class Port
	{
	public:
		enum Type
		{		
			CLOCK,
			RESET,
			DATA
		};

		enum Dir
		{
			IN,
			OUT
		};

		Port(Type type, Dir dir, Node* node);
		~Port();

		PROP_GET_SET(name, const std::string&, m_name);
		PROP_GET_SET(type, Type, m_type);
		PROP_GET_SET(dir, Dir, m_dir);
		PROP_GET_SET(parent, Node*, m_parent);
		PROP_GET_SET(conn, Conn*, m_conn);
		PROP_GET_SET(iface_def, Spec::Interface*, m_iface_def);

		static Dir rev_dir(Dir dir);

	protected:
		Type m_type;
		Dir m_dir;
		std::string m_name;
		Node* m_parent;	
		Conn* m_conn;
		Spec::Interface* m_iface_def;
	};

	class ClockResetPort : public Port
	{
	public:
		ClockResetPort(Type type, Dir dir, Node* node, Spec::Signal* def = nullptr);
		~ClockResetPort();

		PROP_GET_SET(sig_def, Spec::Signal*, m_sigdef);

	protected:
		Spec::Signal* m_sigdef;
	};

	class DataPort : public Port
	{
	public:
		typedef std::vector<Flow*> FlowVec;

		DataPort(Node* node, Dir dir);
		~DataPort();

		PROP_GET_SET(clock, ClockResetPort*, m_clock);
		PROP_GET_SET(proto, const Protocol&, m_proto);

		const FlowVec& flows() { return m_flows; }
		void add_flow(Flow* f);
		void remove_flow(Flow* f);
		bool has_flow(Flow* f);
		void clear_flows();
		void add_flows(const FlowVec& f);

	protected:
		ClockResetPort* m_clock;
		FlowVec m_flows;
		Protocol m_proto;
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


	class Flow
	{
	public:
		typedef std::forward_list<DataPort*> Sinks;

		PROP_GET_SET(id, int, m_id);
		PROP_GET_SET(src, DataPort*, m_src);
		
		DataPort* get_sink();
		void set_sink(DataPort* sink);
		const Sinks& sinks() { return m_sinks; }
		void add_sink(DataPort* sink);
		void remove_sink(DataPort* sink);
		bool has_sink(DataPort* sink);

	protected:
		DataPort* m_src;
		Sinks m_sinks;
		int m_id;
	};
}
}