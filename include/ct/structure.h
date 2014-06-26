#pragma once

#include "ct/common.h"
#include "ct/hierarchy.h"
#include "ct/networks.h"
#include "ct/connections.h"

namespace ct
{
	class NodeDef;
	class PortDef;
	
	class Node;
	class Port;
	class System;

	class PortDef : public HierNode, public Object
	{
	public:
		PortDef();
		virtual ~PortDef();

		PROP_GET_SET(name, const std::string&, m_name);
		PROP_GET_SET(type, NetworkID, m_type);
		PROP_GET_SET(parent, NodeDef*, m_parent);
		PROP_GET_SET(dir, Dir, m_dir);

		virtual const std::string& hier_name() const;
		virtual HierNode* hier_parent() const;

		virtual Port* instantiate() = 0;

	protected:
		NodeDef* m_parent;
		std::string m_name;
		NetworkID m_type;
		Dir m_dir;
	};

	class NodeDef : public HierNode, public Object
	{
	public:
		static const std::string ID; // category ID

		NodeDef();
		virtual ~NodeDef();

		PROP_GET_SET(name, const std::string&, m_name);
		PROP_DICT(PortDefs, port_def, PortDef);

		virtual const std::string& hier_name() const;
		virtual HierNode* hier_parent() const;
		virtual HierNode* hier_child(const std::string& name) const;
		virtual HierNode::Children hier_children() const;

		virtual Node* instantiate();

	protected:
		std::string m_name;
	};

	class Port : public HierNode, public Object, public Connectable
	{
	public:
		Port();
		virtual ~Port();

		PROP_GET_SET(name, const std::string&, m_name);
		PROP_GET_SET(port_def, PortDef*, m_port_def);
		PROP_GET_SET(parent, Node*, m_parent);

		virtual const std::string& hier_name() const;
		virtual HierNode* hier_parent() const;

	protected:
		Node* m_parent;
		PortDef* m_port_def;
		std::string m_name;
	};

	class Node : public HierNode, public Object
	{
	public:
		Node();
		virtual ~Node();

		PROP_GET_SET(name, const std::string&, m_name);
		PROP_GET_SET(node_def, NodeDef*, m_node_def);
		PROP_GET_SET(parent, System*, m_parent);
		PROP_DICT(Ports, port, Port);

		virtual const std::string& hier_name() const;
		virtual HierNode* hier_parent() const;
		virtual HierNode* hier_child (const std::string& name) const;
		virtual HierNode::Children hier_children() const;

	protected:
		System* m_parent;
		NodeDef* m_node_def;
		std::string m_name;
	};

	class System : public HierNode, public Object
	{
	public:
		static const std::string ID; // category ID

		System();
		~System();

		PROP_GET_SET(name, const std::string&, m_name);
		PROP_DICT(Nodes, node, Node);

		virtual const std::string& hier_name() const;
		virtual HierNode* hier_parent() const;
		virtual HierNode* hier_child(const std::string&) const;
		virtual HierNode::Children hier_children() const;

		Conns get_conns(const NetworkID&) const;
		Conn* connect(Endpoint* src, Endpoint* sink);
		Conn* connect(Endpoint* src, const Endpoints& sinks);
		void disconnect(Endpoint* src, Endpoint* sink);
		void disconnect(Conn* conn);
		void disconnect(Endpoint* ep);

	protected:
		Conns m_conns;
		std::string m_name;
	};
}