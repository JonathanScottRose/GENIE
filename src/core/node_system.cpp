#include "pch.h"
#include "node_system.h"

using namespace genie::impl;

NodeSystem::NodeSystem(const std::string & name)
    : Node(name, name)
{
}

Node* NodeSystem::instantiate(const std::string& name)
{
    return new NodeSystem(*this, name);
}

NodeSystem::NodeSystem(const NodeSystem& o, const std::string& name)
    : Node(o, name)
{
}
