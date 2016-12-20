#include "pch.h"
#include "genie/genie.h"
#include "genie_priv.h"
#include "node.h"
#include "util.h"
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
    std::unordered_map<std::string, Node*> m_nodes;

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
        if (util::exists_2(m_nodes, name))
            throw genie::Exception("module with name " + name + " already exists");

        for (auto it : m_nodes)
        {
            if (it.second->get_hdl_name() == hdl_name)
                throw genie::Exception("module with hdl_name " + hdl_name + " already exists");
        }

        // Store it in the context
        m_nodes[name] = node;
        return node;
    }
}

void genie::impl::register_builtin(Node* node)
{
    register_node(node);
}

Node* genie::impl::get_node(const std::string& name)
{
    Node* result = nullptr;

    auto it = m_nodes.find(name);
    if (it != m_nodes.end())
        result = it->second;

    return result;
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

