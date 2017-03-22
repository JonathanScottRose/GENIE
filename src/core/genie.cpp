#include "pch.h"
#include "genie_priv.h"
#include "node.h"
#include "port.h"
#include "util.h"
#include "hierarchy.h"

#include "node_system.h"
#include "node_user.h"
#include "node_split.h"

#include "net_clockreset.h"
#include "net_conduit.h"
#include "net_internal.h"
#include "net_rs.h"
#include "net_topo.h"

#include "port_clockreset.h"
#include "port_conduit.h"
#include "port_rs.h"

#include "genie/genie.h"

using namespace genie::impl;

//
// INTERNAL API
//

namespace
{
    // Holds all nodes
	HierObject m_root;

	// Holds network defs
	std::vector<Network*> m_networks;

	// Holds port defs
	std::vector<PortTypeInfo*> m_port_types;

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

NetType genie::impl::register_network(Network* def)
{
	NetType ret = (NetType)m_networks.size();
	m_networks.push_back(def);
	def->set_id(ret);
	return ret;
}

const Network* genie::impl::get_network(NetType id)
{
	if (id < m_networks.size())
		return m_networks[id];

	return nullptr;
}

PortType genie::impl::register_port_type(PortTypeInfo* info)
{
	PortType ret = (PortType)m_port_types.size();
	m_port_types.push_back(info);
	info->set_id(ret);
	return ret;
}

const PortTypeInfo* genie::impl::get_port_type(PortType id)
{
	if (id < m_port_types.size())
		return m_port_types[id];

	return nullptr;
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

	// Initialize structs
	m_networks.clear();
	m_port_types.clear();

    // Register builtins
	NetClock::init();
	NetReset::init();
	NetConduit::init();
	NetConduitSub::init();
	NetInternal::init();
	NetRSLogical::init();
	NetRS::init();
	NetRSSub::init();

	PortClock::init();
	PortReset::init();
	impl::PortConduit::init();
	PortConduitSub::init();
	impl::PortRS::init();
	PortRSSub::init();

    NodeSplit::init();
}

void genie::shutdown()
{
	for (auto c : m_root.get_children())
	{
		delete m_root.remove_child(c);
	}

	util::delete_all(m_networks);
	util::delete_all(m_port_types);
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

