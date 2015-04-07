#pragma once

#include "genie/networks.h"
#include "genie/structure.h"
#include "genie/net_topo.h"
#include "genie/value.h"

namespace genie
{
	class RSPort;
	class RSLinkpoint;
	class ClockPort;

	extern const NetType NET_RS;

	class RSLink : public Link
	{
	public:
		RSLink() = default;
		~RSLink() = default;

		PROP_GET_SETR(flow_id, Value&, m_flow_id);

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

		TopoPort* get_topo_port() const;
		ClockPort* get_clock_port() const;
		List<RSLinkpoint*> get_linkpoints() const;
		PROP_GET_SET(domain_id, int, m_domain_id);
		PROP_GET_SET(clock_port_name, const std::string&, m_clk_port_name);

		void refine(NetType) override;
		HierObject* instantiate() override;

	protected:
		int m_domain_id;
		std::string m_clk_port_name;

		void refine_topo();
		void refine_rvd();
	};

	class RSLinkpoint : public Port
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
		PROP_GET_SET(encoding, const Value&, m_encoding);

		HierObject* instantiate() override;
		RSPort* get_rs_port() const;

	protected:
		Type m_type;
		Value m_encoding;
	};
}