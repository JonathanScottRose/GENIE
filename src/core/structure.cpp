#include "ct/structure.h"

using namespace ct;

const std::string& PortDef::hier_name() const
{
	return m_name;
}

HierNode* PortDef::hier_parent() const
{
	return m_parent;
}

HierNode* NodeDef::hier_parent() const
{
	return nullptr;
}

HierNode* NodeDef::hier_child(const std::string& name) const
{
	return get_port_def(name);
}

HierNode::Children NodeDef::hier_children() const
{
	return Util::values<Children>(m_port_defs);
}