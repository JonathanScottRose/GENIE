#include "genie/networks.h"

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
		std::unordered_map<NetType, NetTypeDef*> net_types;

		~NetTypeRegistry()
		{
			Util::delete_all_2(net_types);
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

//
// Static
//

NetType NetTypeDef::alloc_def_internal()
{
	// Allocates a new, unique network ID
	static NetType last_net = NET_INVALID;
	return ++last_net;
}

void NetTypeDef::add_def_internal(NetTypeDef* def)
{
	// Store definition in the registry. As a key, use the NetType 
	// that the Def returns.
	NetType id = def->get_id();

	// See if duplicate ID or name already exists
	for (auto& i : s_registry.net_types)
	{
		NetTypeDef* test_def = i.second;
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

NetTypeDef* NetTypeDef::get(NetType id)
{
	auto loc = s_registry.net_types.find(id);
	return loc == s_registry.net_types.end() ? nullptr : loc->second;
}

NetTypeDef* NetTypeDef::get(const std::string& name)
{
	// Do a linear search to find a matching network type by name
	auto loc = s_registry.net_types.begin();
	auto loc_end = s_registry.net_types.end();
	for ( ; loc != loc_end; ++loc)
	{
		if (loc->second->get_name() == name)
			break;
	}

	return loc == loc_end? nullptr : loc->second;
}

//
// Network Definition
//

NetTypeDef::NetTypeDef(NetType id)
: m_id(id), m_ep_asp_id(ASPECT_NULL)
{
}

NetTypeDef::~NetTypeDef()
{
}

Port* NetTypeDef::create_port(Dir dir, const std::string& name, HierObject* parent)
{
	// Default implementation. Should be overridden if your network supports Ports.
	assert(!m_has_port);
	throw Exception("Network " + m_name + " has no instantiable ports");
	return nullptr;
}

Export* NetTypeDef::create_export(Dir dir, const std::string& name, System* parent)
{
	assert(!m_has_port);
	throw Exception("Network " + m_name + " has no instantiable ports");
	return nullptr;
}

PortDef* NetTypeDef::create_port_def(Dir dir, const std::string& name, HierObject* parent)
{
	assert(!m_has_port);
	throw Exception("Network " + m_name + " has no instantiable ports");
	return nullptr;
}