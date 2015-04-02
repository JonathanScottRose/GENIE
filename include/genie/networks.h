#pragma once

#include "genie/common.h"
#include "genie/sigroles.h"

namespace genie
{
	// Defined here
	class Network;

	// Defined elsewhere
	class Port;
	class System;
	class Link;
	class Endpoint;
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

	enum class LinkFace
	{
		INNER,
		OUTER
	};

	// Returns reverse direction
	Dir dir_rev(Dir);
	// Creates a Dir parsed from a string
	Dir dir_from_str(const std::string&);
	// Converts a dir to a string
	const char* dir_to_str(Dir);

	// Network types derive from this abstract class. Being an Object allows backend-specific things to be
	// attached.
	class Network : public Object
	{
	public:
		// Call this in a static initializer to register a network type
		template<class NET_DEF>
		static NetType add()
		{
			NetType id = alloc_def_internal();
			NetTypeRegistry::entries().emplace_front([=]()
			{ 
				return new NET_DEF(id);
			});

			return id;
		}

		// Called by genie::init() to do business
		static void init();

		// Call to retrieve information about a network type
		static Network* get(NetType id);
		static Network* get(const std::string& name);
		static NetType type_from_str(const std::string& name);
		static const std::string& to_string(NetType);

		// Create a new definition with the given id
		Network(NetType id);
		virtual ~Network();

		// Read-only properties
		PROP_GET(id, NetType, m_id);
		PROP_GET(name, std::string&, m_name);
		PROP_GET(desc, std::string&, m_desc);
		PROP_GET(src_multibind, bool, m_src_multibind);
		PROP_GET(sink_multibind, bool, m_sink_multibind);

		// Get allowed signal roles. Role conversion functions.
		typedef std::vector<SigRole> SigRoles;
		const SigRoles& get_sig_roles() const { return m_sig_roles; }
		bool has_sig_role(SigRoleID) const;
		bool has_sig_role(const std::string&) const;
		const SigRole& get_sig_role(SigRoleID) const;
		const SigRole& get_sig_role(const std::string&) const;
		SigRoleID role_id_from_name(const std::string&) const;
		const std::string& role_name_from_id(SigRoleID) const;

		// Factory methods
		virtual Link* create_link();
		virtual Endpoint* create_endpoint(Dir);
		virtual Port* create_port(Dir);

	protected:
		// Descriptive
		std::string m_name;
		std::string m_desc;
		
		// Network properties
		bool m_src_multibind;
		bool m_sink_multibind;

		// Register a signal role and get its ID
		SigRoleID add_sig_role(const SigRole&);

		// Network type registration
		typedef StaticRegistry<Network> NetTypeRegistry;
		static NetType alloc_def_internal();

	private:
		// Made private so that derived classes can't accidentally modify it.
		// It's set correctly in the constructor, presumably by the static registration mechanism
		// via add().
		// If you want its value, just call get_id().
		NetType m_id;

		// Allowable signal roles for ports of this network type
		SigRoles m_sig_roles;
	};
}
