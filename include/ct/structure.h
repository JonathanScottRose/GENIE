#pragma once

#include "ct/common.h"
#include "ct/hierarchy.h"

namespace ct
{
	class NetType;

	class NodeDef;
	class PortDef;
	
	class Node;
	class Port;

	enum class PortDir
	{
		IN,
		OUT
	};

	class NetType
	{
	public:
		bool operator==(const NetType&) const;
		
	protected:
		std::string m_name;
	};

	class PortDef : public HierNode
	{
	public:
		PROP_GET_SET(name, const std::string&, m_name);
		PROP_GET_SET(type, const NetType&, m_type);
		PROP_GET_SET(dir, const PortDir&, m_dir);
		PROP_GET_SET(parent, NodeDef*, m_parent);

		virtual const std::string& hier_name() const;
		virtual HierNode* hier_parent() const;

		virtual Port* instantiate() = 0;

	protected:
		NodeDef* m_parent;
		std::string m_name;
		NetType m_type;
		PortDir m_dir;
	};

	class NodeDef : public HierNode
	{
	public:
		PROP_GET_SET(name, const std::string&, m_name);
		PROP_DICT(PortDefs, port_def, PortDef);

		virtual const std::string& hier_name() const { return m_name; }
		virtual HierNode* hier_parent() const;
		virtual HierNode* hier_child (const std::string& name) const;
		virtual Children hier_children() const;

	protected:
		std::string m_name;
	};
}