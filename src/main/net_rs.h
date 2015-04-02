#pragma once

#include "genie/networks.h"
#include "genie/structure.h"
#include "genie/net_topo.h"
#include "genie/value.h"

namespace genie
{
	class RSPort;

	extern const NetType NET_RS;

	class RSEndpoint : public Endpoint
	{
	public:
		RSEndpoint(Dir);
		~RSEndpoint();

		NetType get_type() const { return NET_RS; }
		RSPort* get_phys_rs_port();
	};

	class RSLink : public Link
	{
	public:
		RSLink() = default;
		~RSLink() = default;

		PROP_GET_SET(flow_id, Value&, m_flow_id);

	protected:
		Value m_flow_id;
	};

	class RSPort : public Port
	{
	public:
		static SigRoleID ROLE_READY;
		static SigRoleID ROLE_VALID;
		static SigRoleID ROLE_DATA;
		static SigRoleID ROLE_DATA_BUNDLE;
		static SigRoleID ROLE_LPID;
		static SigRoleID ROLE_EOP;
		static SigRoleID ROLE_SOP;

		RSPort(Dir dir);
		RSPort(Dir dir, const std::string& name);
		~RSPort();

		void refine(NetType);
		TopoPort* get_topo_port() const;
		PROP_GET_SET(domain_id, int, m_domain_id);

		HierObject* instantiate() override;

	protected:
		int m_domain_id;

		void refine_topo();
		void refine_rvd();
	};

	class RSLinkpoint : public HierObject
	{
	public:
		enum Type
		{
			INVALID,
			BROADCAST,
			UNICAST,
			MULTICAST
		};

		static Type type_from_str(const char*);

		RSLinkpoint(Dir, Type);
		~RSLinkpoint();

		PROP_GET(type, Type, m_type);
		PROP_GET(dir, Dir, m_dir);
		PROP_GET_SET(encoding, Value&, m_encoding);

		HierObject* instantiate() override;
		RSPort* get_rs_port() const;

	protected:
		Dir m_dir;
		Type m_type;
		Value m_encoding;
	};
}