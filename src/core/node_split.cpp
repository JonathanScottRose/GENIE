#include "pch.h"
#include "node_split.h"
#include "genie_priv.h"

using namespace genie::impl;

namespace
{
    const char MODNAME[] = "genie_split";
}

void NodeSplit::init()
{
	//todo
}

NodeSplit::NodeSplit()
    : Node(MODNAME, MODNAME)
{
}

NodeSplit::NodeSplit(const NodeSplit& o)
    : Node(o)
{
    // Create a copy of an existing NodeSplit
}

Node* NodeSplit::instantiate()
{
    return new NodeSplit(*this);
}