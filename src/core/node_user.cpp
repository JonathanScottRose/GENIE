#include "pch.h"
#include "node_user.h"

using namespace genie::impl;

NodeUser::NodeUser(const std::string & name, const std::string & hdl_name)
    : Node(name, hdl_name)
{
}

Node* NodeUser::instantiate(const std::string& name)
{
    return new NodeUser(*this, name);
}

NodeUser::NodeUser(const NodeUser& o, const std::string& name)
    : Node(o, name)
{
}
