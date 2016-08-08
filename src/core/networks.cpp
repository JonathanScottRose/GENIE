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
    static NetType s_last_net_type;

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

void Network::init()
{
    s_registry.net_types.clear();
    s_last_net_type = 0;
}

NetType Network::reg_internal(Network* def)
{
    NetType id = s_last_net_type++;

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

    def->m_id = id;
	s_registry.net_types.emplace(id, def);

    return id;
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

void Network::add_sig_role(SigRoleID id)
{
	const std::string& name = SigRole::get(id)->get_name();

	// Make sure name is unique
	auto roles = get_sig_roles();
	for (const auto& entry : roles)
	{
		if (entry->get_name() == name)
			throw Exception("signal role with name " + name + " already defined");
	}

	// Add the role by ID
	m_sig_roles.push_back(id);
}

bool Network::has_sig_role(SigRoleID id) const
{
	return util::exists(m_sig_roles, id);
}

bool Network::has_sig_role(const std::string& name) const
{
	return get_sig_role(name) != nullptr;
}

SigRoleID Network::role_id_from_name(const std::string& name) const
{
	// Do a case-insensitive comparison. Entries are lowercase, and make the name lowercase too.
	std::string lname = util::str_tolower(name);

	for (auto id: m_sig_roles)
	{
		auto role = SigRole::get(id);
		if (role->get_name() == lname)
			return role->get_id();
	}

	return ROLE_INVALID;
}

const std::string& Network::role_name_from_id(SigRoleID id) const
{
	return get_sig_role(id)->get_name();
}

const SigRole* Network::get_sig_role(SigRoleID id) const
{
	auto it = std::find(m_sig_roles.begin(), m_sig_roles.end(), id);

	return (it == m_sig_roles.end())? nullptr : SigRole::get(id);
}

const SigRole* Network::get_sig_role(const std::string& name) const
{
	auto id = role_id_from_name(name);
	if (id == ROLE_INVALID)
		return nullptr;

	return SigRole::get(id);
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

List<const SigRole*> Network::get_sig_roles() const
{
	List<const SigRole*> result;

	for (auto id : m_sig_roles)
		result.push_back(SigRole::get(id));

	return result;
}