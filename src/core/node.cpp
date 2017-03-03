#include "pch.h"
#include "node.h"
#include "params.h"
#include "port_clockreset.h"
#include "port_conduit.h"
#include "port_rs.h"
#include "genie/genie.h"

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

    // Used for clock and reset ports
    template<class P>
    genie::Port * create_simple_port(Node* node, const std::string & name, genie::Port::Dir dir, 
        const std::string & hdl_sig)
    {
        // Create the port
        auto result = new P(name, dir);

        // Create HDL port
        node->get_hdl_state().add_port(hdl_sig, 1, 1, hdl::Port::from_logical_dir(dir));

        // Create binding (using defaults for 1-bit binding)
        hdl::PortBindingRef binding;
        binding.set_port_name(hdl_sig);
        result->set_binding(binding);

        // Add port to hierarchy
        node->add_child(result);

        return result;
    }
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

genie::Port * Node::create_clock_port(const std::string & name, genie::Port::Dir dir, 
    const std::string & hdl_sig)
{
    return create_simple_port<ClockPort>(this, name, dir, hdl_sig);
}

genie::Port * Node::create_reset_port(const std::string & name, genie::Port::Dir dir, 
    const std::string & hdl_sig)
{
    return create_simple_port<ResetPort>(this, name, dir, hdl_sig);
}

genie::ConduitPort * Node::create_conduit_port(const std::string & name, genie::Port::Dir dir)
{
    auto result = new ConduitPort(name, dir);
    add_child(result);
    return result;
}

genie::RSPort * Node::create_rs_port(const std::string & name, genie::Port::Dir dir, 
    const std::string & clk_port_name)
{
    auto result = new RSPort(name, dir);
    result->set_clk_port_name(clk_port_name);
    return result;
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

    // First, concreteize any expressions within the parameters themselves
    for (auto& p : m_params)
    {
        p.second->resolve(resolv);
    }

    // Resolve expresisons within child ports
    auto ports = get_children_by_type<Port>();
    for (auto p : ports)
    {
        p->resolve_params(resolv);
    }

    // Resolve expressions within HDL-related stuff
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
