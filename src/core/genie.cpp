#include "pch.h"
#include "genie/genie.h"
#include "genie_priv.h"
#include "node.h"
#include "util.h"
#include "hierarchy.h"
#include "node_system.h"
#include "node_user.h"
#include "node_split.h"


using namespace genie::impl;

//
// INTERNAL API
//

namespace
{
    // Holds all nodes
	HierObject m_root;

    // Stores options
    genie::FlowOptions m_flow_opts;
    genie::ArchParams m_arch_params;

    //
    // LOCAL FUNCTIONS
    //
    Node* register_node(Node* node)
    {
        const std::string& name = node->get_name();
        const std::string& hdl_name = node->get_hdl_name();

        // Check for existing names
        if (m_root.has_child(name))
            throw genie::Exception("module with name " + name + " already exists");

        for (auto node : m_root.get_children_by_type<Node>())
        {
            if (node->get_hdl_name() == hdl_name)
                throw genie::Exception("module with hdl_name " + hdl_name + " already exists");
        }

		m_root.add_child(node);

        return node;
    }
}

void genie::impl::register_builtin(Node* node)
{
    register_node(node);
}

Node* genie::impl::get_node(const std::string& name)
{
	return m_root.get_child_as<Node>(name);
}

std::vector<NodeSystem*> genie::impl::get_systems()
{
    return m_root.get_children_by_type<NodeSystem>();
}

void genie::impl::delete_node(Node * node)
{
	auto obj = m_root.remove_child(node);
	if (obj)
		delete obj;
}

void genie::impl::delete_node(const std::string & name)
{
	auto obj = m_root.remove_child(name);
	if (obj)
		delete obj;
}

genie::FlowOptions& genie::impl::get_flow_options()
{
    return m_flow_opts;
}

genie::ArchParams& genie::impl::get_arch_params()
{
    return m_arch_params;
}

// 
// PUBLIC API
//

void genie::init(genie::FlowOptions* opts, genie::ArchParams* arch)
{
    // Override default options
    if (opts)
        m_flow_opts = *opts;

    if (arch)
        m_arch_params = *arch;

    // Register builtins
    NodeSplit::init();
}

genie::Node * genie::create_system(const std::string & name)
{
    auto node = new NodeSystem(name);
    try
    {
        register_node(node);
    }
    catch (...)
    {
        delete node;
        throw;
    }
    return node;
}

genie::Node * genie::create_module(const std::string & name)
{
    return create_module(name, name);
}

genie::Node * genie::create_module(const std::string & name, const std::string & hdl_name)
{
    auto node = new NodeUser(name, hdl_name);
    try
    {
        register_node(node);
    }
    catch(...)
    {
        delete node;
        throw;
    }
    return node;
}

