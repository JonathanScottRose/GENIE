#pragma once

#include "genie/common.h"

namespace genie
{
	// Defined here
	class NetTypeDef;

	// Defined elsewhere
	class Port;
	class PortDef;
	class Export;
	class System;
	class Link;
	class AEndpoint;
	class HierObject;

	// Lightweight identifier to refer to a Network Type.
	typedef unsigned int NetType;
	const NetType NET_INVALID = 0;

	// Directionality enum for endpoints and ports
	enum class Dir
	{
		INVALID,
		IN,
		OUT
	};

	// Returns reverse direction
	Dir dir_rev(Dir);
	// Creates a Dir parsed from a string
	Dir dir_from_str(const std::string&);
	// Converts a dir to a string
	const char* dir_to_str(Dir);

	// Network types derive from this abstract class
	class NetTypeDef
	{
	public:
		// Call this in a static initializer to register a network type
		template<class NET_DEF>
		static NetType add()
		{
			NetType id = alloc_def_internal();
			add_def_internal(new NET_DEF(id));
			return id;
		}

		// Call to retrieve information about a network type
		static NetTypeDef* get(NetType id);
		static NetTypeDef* get(const std::string& name);
		static NetType type_from_str(const std::string& name);

		// Create a new definition with the given id
		NetTypeDef(NetType id);
		virtual ~NetTypeDef();

		// Read-only properties
		PROP_GET(id, NetType, m_id);
		PROP_GET(name, const std::string&, m_name);
		PROP_GET(desc, const std::string&, m_desc);
		PROP_GET(has_port, bool, m_has_port);
		PROP_GET(src_multibind, bool, m_src_multibind);
		PROP_GET(sink_multibind, bool, m_sink_multibind);

		// Gets the AspectID for the AEndpoint subclass for this network type
		PROP_GET(ep_asp_id, AspectID, m_ep_asp_id);

		// Factory methods
		virtual Link* create_link() = 0;
		virtual AEndpoint* create_endpoint(Dir, HierObject*) = 0;
		virtual Port* create_port(Dir, const std::string&, HierObject*);
		virtual PortDef* create_port_def(Dir, const std::string&, HierObject*);
		virtual Export* create_export(Dir, const std::string&, System*);

	protected:
		// Descriptive
		std::string m_name;
		std::string m_desc;
		
		// Network properties
		bool m_has_port;
		bool m_src_multibind;
		bool m_sink_multibind;
		AspectID m_ep_asp_id;

		// Network type registration
		static void add_def_internal(NetTypeDef* def);
		static NetType alloc_def_internal();

	private:
		// Made private so that derived classes can't accidentally modify it.
		// It's set correctly in the constructor, presumably by the static registration mechanism
		// via add().
		// If you want its value, just call get_id().
		NetType m_id;
	};
}
