#include "pch.h"
#include "genie/genie.h"
#include "node.h"
#include "node_params.h"

using namespace genie::impl;


//
// Public functions
//

void Node::set_int_param(const std::string& parm_name, int val)
{
	set_param(parm_name, new NodeIntParam(val));
}

void Node::set_int_param(const std::string& parm_name, const std::string & expr)
{
	try
	{
		set_param(parm_name, new NodeIntParam(expr));
	}
	catch (Exception& e)
	{
		throw Exception("Parameter " + parm_name + " of " + get_hier_path() + ": " + e.what());
	}
}

void Node::set_str_param(const std::string & parm_name, const std::string & str)
{
	set_param(parm_name, new NodeStringParam(str));
}

//
// Internal functions
//

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

	// Parameters and values
	for (auto it : o.m_params)
	{
		m_params[it.first] = it.second->clone();
	}

}

//
// Protected functions
//

void Node::set_param(const std::string& name, NodeParam * param)
{
	// Uppercase the name
	std::string name_up = util::str_toupper(name);

	// Check for existing param
	auto it = m_params.find(name_up);
	if (it != m_params.end())
	{
		auto old_param = it->second;

		// Check if new param's value is of different type than existing and throw warning
		if (param->get_type() != old_param->get_type())
		{
			log::warn("%s: redefining parameter %s with different basic type",
				this->get_hier_path(), name.c_str());
		}
		
		delete old_param;
	}

	m_params[name] = param;
}

Node::~Node()
{
}
