#include "pch.h"
#include "genie/genie.h"
#include "node.h"

using namespace genie::impl;

Node::Node(const std::string & name, const std::string & hdl_name)
    : m_name(name), m_hdl_name(hdl_name), m_parent(nullptr)
{
}

void Node::set_parent(Node* parent)
{
    // Can't just move things!
    if (m_parent != nullptr)
    {
        // TODO: throw exception with full node path
        throw Exception("node already has a parent");
    }

    m_parent = parent;
}

Node::Node(const Node& o, const std::string& name)
    : m_name(name), m_hdl_name(o.m_hdl_name)
{
    // Copy over all the things that every Node has
}

Node::~Node()
{
}
