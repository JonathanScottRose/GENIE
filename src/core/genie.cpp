#include "pch.h"
#include "genie_priv.h"
#include "node.h"
#include "port.h"
#include "util.h"
#include "hierarchy.h"
#include "sig_role.h"
#include "prim_db.h"

#include "node_system.h"
#include "node_user.h"
#include "node_split.h"
#include "node_merge.h"
#include "node_conv.h"
#include "node_clockx.h"
#include "node_mdelay.h"
#include "node_reg.h"

#include "net_clockreset.h"
#include "net_conduit.h"
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
	class HierRoot : public HierObject
	{
	public:
		HierObject* clone() const override
		{
			// Cannot clone root
			assert(false);
			return nullptr;
		}
	};

    // Holds all nodes
	HierRoot m_root;

	// Reserved modules
	std::vector<std::string> m_reserved_modules;

	// Holds network defs
	std::vector<NetworkDef*> m_networks;

	// Holds port defs
	std::vector<PortTypeDef*> m_port_types;

	// Holds field definitions (later. just integer now)
	FieldType m_next_field_id;

	// Holds signal role definitions
	std::vector<SigRoleDef*> m_sig_roles;

	// Primitive database
	std::unordered_map<std::string, PrimDB*> m_prim_db;

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

		// Check for reserved names
		if (is_reserved_module(hdl_name))
			throw genie::Exception("HDL module name " + hdl_name + " is reserved");

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

PrimDB* genie::impl::load_prim_db(const std::string & modname, 
	const SmartEnumTable & col_enum, const SmartEnumTable & tnode_src_enum, 
	const SmartEnumTable & tnode_sink_enum)
{
	std::string filename = util::get_exe_path() + "../data/" + modname + ".txt";
	
	auto result = new PrimDB;
	result->initialize(filename, col_enum, tnode_src_enum, tnode_sink_enum);

	auto ins_result = m_prim_db.emplace(std::make_pair(modname, result));
	if (!ins_result.second)
		throw genie::Exception("prim database for " + modname + " already exists");

	return result;
}

void genie::impl::register_reserved_module(const std::string& name)
{
	if (!is_reserved_module(name))
	{
		m_reserved_modules.push_back(name);
	}
}

bool genie::impl::is_reserved_module(const std::string& name)
{
	return util::exists(m_reserved_modules, name);
}

HierObject * genie::impl::get_object(const std::string & path)
{
	return m_root.get_child_as<HierObject>(path);
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

NetType genie::impl::register_network(NetworkDef* def)
{
	NetType ret = (NetType)m_networks.size();
	m_networks.push_back(def);
	def->set_id(ret);
	return ret;
}

const NetworkDef* genie::impl::get_network(NetType id)
{
	if (id < m_networks.size())
		return m_networks[id];

	return nullptr;
}

const NetworkDef * genie::impl::get_network(const std::string & name)
{
	for (auto net : m_networks)
	{
		if (net->get_name() == name)
			return net;
	}
	return nullptr;
}

PortType genie::impl::register_port_type(PortTypeDef* info)
{
	PortType ret = (PortType)m_port_types.size();
	m_port_types.push_back(info);
	info->set_id(ret);
	return ret;
}

const PortTypeDef* genie::impl::get_port_type(PortType id)
{
	if (id < m_port_types.size())
		return m_port_types[id];

	return nullptr;
}

FieldType genie::impl::register_field()
{
	// Fancier registration to be done later
	return m_next_field_id++;
}

SigRoleType genie::impl::register_sig_role(SigRoleDef* def)
{
	SigRoleType result = m_sig_roles.size();
	m_sig_roles.push_back(def);
	return result;
}

const SigRoleDef* genie::impl::get_sig_role(SigRoleType type)
{
	return m_sig_roles[(unsigned)type];
}

const SigRoleType genie::impl::get_sig_role_from_str(const std::string& str)
{
	auto result = SigRoleType::INVALID;

	auto str_lc = util::str_tolower(str);

	for (unsigned i = 0; i < m_sig_roles.size(); i++)
	{
		auto def = m_sig_roles[i];
		if (def->get_name() == str_lc)
		{
			result = i;
			break;
		}
	}

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

genie::APIObject::~APIObject() {}

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
	m_reserved_modules.clear();
	m_next_field_id = 0;

    // Register builtins
	NetClock::init();
	NetReset::init();
	NetConduit::init();
	NetTopo::init();
	NetRSLogical::init();
	NetRSPhys::init();

	impl::PortClock::init();
	impl::PortReset::init();
	impl::PortConduit::init();
	impl::PortRS::init();

    NodeSplit::init();
	NodeMerge::init();
	NodeConv::init();
	NodeClockX::init();
	NodeMDelay::init();
	NodeReg::init();
}

void genie::shutdown()
{
	for (auto c : m_root.get_children())
	{
		delete m_root.remove_child(c);
	}

	util::delete_all(m_networks);
	util::delete_all(m_port_types);
	util::delete_all(m_sig_roles);
	util::delete_all_2(m_prim_db);

	m_reserved_modules.clear();
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

