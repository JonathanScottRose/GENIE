#pragma once

#include "genie/common.h"
#include "genie/hierarchy.h"
#include "genie/networks.h"
#include "connections.h"

namespace genie
{
	class NodeDef;
	class PortDef;
	class Node;
	class Port;
	class System;
	class HierRoot;

	class PortDef : public HierObject
	{
	public:
		PortDef(Dir dir);
		PortDef(Dir dir, const std::string& name, HierObject* parent = nullptr);
		virtual ~PortDef() = default;

		virtual NetType get_type() const = 0;
		PROP_GET(dir, Dir, m_dir);

	protected:
		Dir m_dir;
	};

	class NodeDef : public HierObject
	{
	public:
		typedef List<PortDef*> PortDefs;

		NodeDef();
		NodeDef(const std::string& name, HierObject* parent = nullptr);
		virtual ~NodeDef() = default;

		PortDefs get_port_defs() const;
		PortDef* get_port_def(const std::string&) const;
		void add_port_def(PortDef*);
		void remove_port_def(const std::string&);
	};

	class Port : public HierObject
	{
	public:
		Port(Dir);
		Port(Dir, const std::string& name, HierObject* parent = nullptr);
		virtual ~Port() = default;

		virtual NetType get_type() const = 0;
		PROP_GET(dir, Dir, m_dir);

	protected:
		Dir m_dir;
	};

	class Export : public HierObject
	{
	public:
		Export(Dir dir);
		Export(Dir dir, const std::string& name, System* parent = nullptr);
		virtual ~Export() = 0;

		virtual NetType get_type() const = 0;
		PROP_GET(dir, Dir, m_dir);

	protected:
		Dir m_dir;
	};

	class Node : public HierObject
	{
	public:
		typedef List<Port*> Ports;

		Node();
		Node(const std::string& name, HierObject* parent = nullptr);
		virtual ~Node() = default;

		Ports get_ports(NetType) const;
		Ports get_ports() const;
		Port* get_port(const std::string& name) const;
		void add_port(Port*);
		void delete_port(const std::string& name);
	};

	class System : public HierObject
	{
	public:
		typedef List<Link*> Links;
		typedef List<NetType> NetTypes;
		typedef List<Node*> Nodes;
		typedef List<Export*> Exports;
		typedef List<HierObject*> Objects;

		System();
		System(const std::string& name);
		virtual ~System();

		HierObject* instantiate();

		NetTypes get_net_types() const;

		Links get_links() const;
		Links get_links(NetType) const;
		Links get_links(HierObject* src, HierObject* sink) const;
		Links get_links(HierObject* src, HierObject* sink, NetType net) const;
		Link* connect(HierObject* src, HierObject* sink);
		Link* connect(HierObject* src, HierObject* sink, NetType net);
		void disconnect(HierObject* src, HierObject* sink);
		void disconnect(HierObject* src, HierObject* sink, NetType net);
		void disconnect(Link*);
		void splice(Link* orig, HierObject* new_sink, HierObject* new_src);

		Objects get_objects() const;
		Nodes get_nodes() const;
		Exports get_exports() const;

		void add_object(HierObject*);
		HierObject* get_object(const HierPath& path) const;
		void delete_object(const HierPath& path);
		
	protected:
		NetType find_auto_net_type(HierObject*, HierObject*) const;
		void get_eps(HierObject*, HierObject*, NetType, AEndpoint*&, AEndpoint*&) const;

		std::unordered_map<NetType, Links> m_links;
	};

	class HierRoot : public HierObject
	{
	public:
		typedef List<HierObject*> Objects;
		typedef List<System*> Systems;
		typedef List<NodeDef*> NodeDefs;

		HierRoot();
		~HierRoot();

		void add_object(HierObject*);
		HierObject* get_object(const HierPath&);

		const std::string& get_name() const;
		void set_name(const std::string&) { }
		void set_parent(HierObject*) { }
		void set_prototype(HierObject*) { }

		Systems get_systems();
		Objects get_objects();
		NodeDefs get_node_defs();
	};
}