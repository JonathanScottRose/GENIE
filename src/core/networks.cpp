#include "genie/networks.h"
#include "genie/connections.h"
#include "genie/structure.h"

#include "genie/vlog.h" // uuugly

using namespace genie;

//
// Module-local
//
namespace
{
	// Holds all registered network types. Instantiated once, here. Upon program unload,
	// destructor is called to free registered types.
	struct NetTypeRegistry
	{
		std::unordered_map<NetType, Network*> net_types;

		~NetTypeRegistry()
		{
			util::delete_all_2(net_types);
		}
	} s_registry;
}

//
// Global
//

Dir genie::dir_rev(Dir dir)
{
	switch(dir)
	{
		case Dir::IN: return Dir::OUT;
		case Dir::OUT: return Dir::IN;
		default: return Dir::INVALID;
	}
}

namespace
{
	const std::vector<std::pair<Dir, const char*>> s_dir_table =
	{
		{ Dir::IN, "IN"},
		{ Dir::OUT, "OUT" }
	};
}

Dir genie::dir_from_str(const std::string& str)
{
	

	Dir result = Dir::INVALID;
	util::str_to_enum(s_dir_table, str, &result);
	return result;
}

const char* genie::dir_to_str(Dir dir)
{
	return util::enum_to_str(s_dir_table, dir);
}

//
// Static
//

NetType Network::alloc_def_internal()
{
	// Allocates a new, unique network ID
	static NetType last_net = 0;
	return last_net++;
}

void Network::init()
{
	for (auto& net_reg_func : NetTypeRegistry::entries())
	{
		// Call the Network's constructor
		auto def = net_reg_func();

		// Store definition in the registry. As a key, use the NetType 
		// that the Def returns.
		NetType id = def->get_id();

		// See if duplicate ID or name already exists
		for (auto& i : s_registry.net_types)
		{
			Network* test_def = i.second;
			NetType test_id = i.first;

			if (test_id == id || def->get_name() == test_def->get_name())
			{
				throw Exception(
					"Can't register network " + 
					def->get_name() + " (ID " + std::to_string(id) + ") " +
					" because network " +
					test_def->get_name() + " (ID " + std::to_string(test_id) + ") " +
					" is already registered"
					);
			}
			
		}

		// Register def
		s_registry.net_types.emplace(id, def);
	}
}

Network* Network::get(NetType id)
{
	auto loc = s_registry.net_types.find(id);
	return loc == s_registry.net_types.end() ? nullptr : loc->second;
}

Network* Network::get(const std::string& name)
{
	std::string uname = util::str_tolower(name);

	// Do a linear search to find a matching network type by name
	auto loc = s_registry.net_types.begin();
	auto loc_end = s_registry.net_types.end();
	for ( ; loc != loc_end; ++loc)
	{
		if (loc->second->get_name() == uname)
			break;
	}

	return loc == loc_end? nullptr : loc->second;
}

NetType Network::type_from_str(const std::string& name)
{
	// Do a linear search to find a matching network type by name
	auto loc = s_registry.net_types.begin();
	auto loc_end = s_registry.net_types.end();
	for ( ; loc != loc_end; ++loc)
	{
		if (loc->second->get_name() == name)
			break;
	}

	return loc == loc_end? NET_INVALID : loc->first;
}

const std::string& Network::to_string(NetType type)
{
	static std::string INV = "<invalid network>";
	auto def = get(type);

	return def? INV : def->get_name();
}

//
// Network Definition
//

Network::Network(NetType id)
: m_id(id)
{
}

Network::~Network()
{
}

Port* Network::create_port(Dir dir)
{
	// Default implementation. Should be overridden if your network supports Ports.
	throw Exception("Network " + m_name + " has no instantiable ports");
	return nullptr;
}

Link* Network::create_link()
{
	// Generic link
	return new Link();
}

SigRoleID Network::add_sig_role(const SigRole& rs)
{
	// Calculate a Role ID for the new entry
	SigRoleID new_id = static_cast<SigRoleID>(m_sig_roles.size());

	// Make the name lowercase
	std::string new_name = util::str_tolower(rs.get_name());

	// Make sure name is unique
	for (const auto& entry : m_sig_roles)
	{
		if (entry.get_name() == new_name)
			throw Exception("signal role with name " + new_name + " already defined");
	}

	// Copy the rs into our internal vector
	m_sig_roles.emplace_back(rs);

	// Assign the new id (and lowercased name) to the new SigRole that's now in the vector
	auto& entry = m_sig_roles.back();
	entry.set_id(new_id);
	entry.set_name(new_name);

	return new_id;
}

bool Network::has_sig_role(SigRoleID id) const
{
	for (const auto& entry : m_sig_roles)
	{
		if (entry.get_id() == id)
			return true;
	}

	return false;
}

bool Network::has_sig_role(const std::string& name) const
{
	std::string lname = util::str_tolower(name);
	for (const auto& entry : m_sig_roles)
	{
		if (entry.get_name() == lname)
			return true;
	}

	return false;
}

SigRoleID Network::role_id_from_name(const std::string& name) const
{
	// Do a case-insensitive comparison. Entries are lowercase, and make the name lowercase too.
	std::string lname = util::str_tolower(name);

	for (const auto& role : m_sig_roles)
	{
		if (role.get_name() == lname)
			return role.get_id();
	}

	return ROLE_INVALID;
}

const std::string& Network::role_name_from_id(SigRoleID id) const
{
	return get_sig_role(id).get_name();
}

const SigRole& Network::get_sig_role(SigRoleID id) const
{
	if (id >= m_sig_roles.size())
		throw Exception("invalid role ID " + std::to_string(id));

	return m_sig_roles[id];
}

const SigRole& Network::get_sig_role(const std::string& name) const
{
	auto id = role_id_from_name(name);
	if (id == ROLE_INVALID)
		throw Exception("invalid signal role: " + name);

	return get_sig_role(id);
}

Port* Network::export_port(System* sys, Port* port, const std::string& name)
{
	// Generic exporting behaviour. Start with a fresh new port of the same dir as the old one.
	Port* result = create_port(port->get_dir());
	result->set_name(name);

	// Attach it to the System
	sys->add_port(result);

	// Export the old port's role bindings
	for (auto& old_rb : port->get_role_bindings())
	{
		// Create a RoleBinding of same role/tag but no hdl binding yet.
		auto new_rb = new RoleBinding(old_rb->get_id(), old_rb->get_tag());

		// Export the HDL binding (need to abstract this...)
		auto new_hdlb = vlog::export_binding(sys, result, old_rb->get_hdl_binding());
		new_rb->set_hdl_binding(new_hdlb);

		// Add to the exported port
		result->add_role_binding(new_rb);
	}

	// Make a Link between the old port and the new one
	sys->connect(port, result, port->get_type());

	return result;
}