#include "pch.h"
#include "node_system.h"

using namespace genie::impl;

void NodeSystem::create_sys_param(const std::string & name)
{
    set_param(name, new NodeSysParam());
}

NodeSystem::NodeSystem(const std::string & name)
    : Node(name, name)
{
}

Node* NodeSystem::instantiate(const std::string& name)
{
    return new NodeSystem(*this, name);
}

std::vector<Node*> NodeSystem::get_nodes() const
{
    return get_children_by_type<Node>();
}

NodeSystem::NodeSystem(const NodeSystem& o, const std::string& name)
    : Node(o, name)
{
}
