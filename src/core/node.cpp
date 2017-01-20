#include "pch.h"
#include "genie/genie.h"
#include "node.h"
#include "params.h"

using namespace genie::impl;
using genie::Exception;

namespace
{
    // Concrete ParamResolver implementation for Nodes
    class NodeParamResolver : public ParamResolver
    {
    protected:
        Node* n;
    public:
        NodeParamResolver(Node* _n): n(_n) {}
        NodeParam* resolve(const std::string& name) override
        {
            // Look for parameter in parent node first before
            // checking this node. Do not recurse further.
            NodeParam* result = nullptr;
            Node* p = n->get_parent_node();
            if (p)
                result = p->get_param(name);

            if (!result)
                result = n->get_param(name);

            if (!result)
                throw Exception(n->get_hier_path() + ": unresolved parameter " + name);

            return result;
        }
    };
}

//
// Public functions
//

const std::string & genie::impl::Node::get_name() const
{
    return HierObject::get_name();
}

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

void Node::set_lit_param(const std::string & parm_name, const std::string & str)
{
    set_param(parm_name, new NodeLiteralParam(str));
}

//
// Internal functions
//

Node::Node(const std::string & name, const std::string & hdl_name)
    : m_hdl_name(hdl_name), m_hdl_state(this)
{
	set_name(name);
}

Node::Node(const Node& o, const std::string& name)
    : HierObject(o), m_hdl_name(o.m_hdl_name),
    m_hdl_state(o.m_hdl_state)
{
	set_name(name);

    // Copy over all the things that every Node has

	// Parameters and values
	for (auto it : o.m_params)
	{
		m_params[it.first] = it.second->clone();
	}

    // Point HDL state back at us
    m_hdl_state.set_node(this);
}

Node * Node::get_parent_node() const
{
    Node* result = const_cast<Node*>(this);
    while (result = dynamic_cast<Node*>(result->get_parent()))
        ;

    return result;
}

void Node::resolve_params()
{
    NodeParamResolver resolv(this);

    for (auto& p : m_params)
    {
        p.second->resolve(resolv);
    }

    m_hdl_state.resolve_params(resolv);
}


NodeParam* Node::get_param(const std::string & name)
{
    auto it = (m_params.find(name));
    if (it == m_params.end())
    {
        return nullptr;
    }
    else
    {
        return it->second;
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
