#include "pch.h"
#include "node_split.h"
#include "genie_priv.h"

using namespace genie::impl;

namespace
{
    const char MODNAME[] = "genie_split";
    NodeSplit* s_proto = nullptr;
}

void NodeSplit::init()
{
    s_proto = new NodeSplit();
    impl::register_builtin(s_proto);
}

NodeSplit * NodeSplit::get_prototype()
{
    return s_proto;
}

NodeSplit::NodeSplit()
    : Node(MODNAME, MODNAME)
{
    // Build the prototype
}

NodeSplit::NodeSplit(const NodeSplit& o, const std::string& name)
    : Node(o, name)
{
    // Create a copy of an existing NodeSplit
}

NodeSplit::NodeSplit(const std::string& name)
    : NodeSplit(*s_proto, name)
{
    // Creates a fresh NodeSplit by copying the prototype
}

Node* NodeSplit::instantiate(const std::string& name)
{
    return new NodeSplit(*this, name);
}