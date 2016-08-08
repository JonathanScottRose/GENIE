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
	const NetType NET_INVALID = std::numeric_limits<NetType>::max();

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
		// Called by genie::init() to do business
		static void init();
        // Call to register a new network type
        template<class T> static NetType reg()
        {
            return reg_internal(new T());
        }

		// Call to retrieve information about a network type
		static Network* get(NetType id);
		static Network* get(const std::string& name);
		static NetType type_from_str(const std::string& name);
		static const std::string& to_string(NetType);

		// Virtual destructor
		virtual ~Network() = default;

		// Read-only properties
		PROP_GET(id, NetType, m_id);
		PROP_GET(name, const std::string&, m_name);
		PROP_GET(desc, const std::string&, m_desc);
		PROP_GET(default_max_in, unsigned int, m_default_max_in);
		PROP_GET(default_max_out, unsigned int, m_default_max_out);

		// Get allowed signal roles. Role conversion functions.
		List<const SigRole*> get_sig_roles() const;
		bool has_sig_role(SigRoleID) const;
		bool has_sig_role(const std::string&) const;
		const SigRole* get_sig_role(SigRoleID) const;
		const SigRole* get_sig_role(const std::string&) const;
		SigRoleID role_id_from_name(const std::string&) const;
		const std::string& role_name_from_id(SigRoleID) const;

		// Factory methods
		virtual Link* create_link();
		virtual Port* create_port(Dir);

		// Network-specific tasks
		virtual Port* export_port(System*, Port*, const std::string&);

	protected:
		// Descriptive
		std::string m_name;
		std::string m_desc;

		// Connection properties
		unsigned int m_default_max_in;
		unsigned int m_default_max_out;

		// Register an allowable signal role for ports of this network type
		void add_sig_role(SigRoleID);

	private:
        static NetType reg_internal(Network* n);

		// Made private so that derived classes can't accidentally modify it.
		// It's set correctly in the constructor, presumably by the static registration mechanism
		// via add().
		// If you want its value, just call get_id().
		NetType m_id;

		// Allowable signal roles for ports of this network type
		List<SigRoleID> m_sig_roles;
	};
}
