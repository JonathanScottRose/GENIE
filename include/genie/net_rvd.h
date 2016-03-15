#pragma once

#include "genie/networks.h"
#include "genie/structure.h"
#include "genie/protocol.h"

namespace genie
{
	class ClockPort;

	// Globally-accessible network type ID. Maybe move to a static member?
	extern const NetType NET_RVD;

	class RVDPort : public Port
	{
	public:
		static const SigRoleID ROLE_VALID;
		static const SigRoleID ROLE_READY;
		static const SigRoleID ROLE_DATA;
		static const SigRoleID ROLE_DATA_CARRIER;

		RVDPort(Dir dir);
		RVDPort(Dir dir, const std::string& name);
		~RVDPort();

		PROP_GET_SET(clock_port_name, const std::string&, m_clk_port_name);
		ClockPort* get_clock_port() const;
		PortProtocol& get_proto() { return m_proto; }

		HierObject* instantiate() override;

	protected:
		PortProtocol m_proto;
		std::string m_clk_port_name;
	};

	class RVDLink : public Link
	{
	public:
		PROP_GET_SET(latency, int, m_latency);

	protected:
		int m_latency = 0;
	};
}