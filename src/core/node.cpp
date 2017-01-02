#include "pch.h"
#include "genie/genie.h"
#include "node.h"

using namespace genie::impl;

Node::Node(const std::string & name, const std::string & hdl_name)
    : m_hdl_name(hdl_name)
{
	set_name(name);
}

Node::Node(const Node& o, const std::string& name)
    : HierObject(o), m_hdl_name(o.m_hdl_name)
{
	set_name(name);
    // Copy over all the things that every Node has
}

Node::~Node()
{
}
