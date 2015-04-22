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
		static SigRoleID ROLE_VALID;
		static SigRoleID ROLE_READY;
		static SigRoleID ROLE_DATA;
		static SigRoleID ROLE_DATA_CARRIER;

		RVDPort(Dir dir);
		RVDPort(Dir dir, const std::string& name);
		~RVDPort();

		PROP_GET_SET(clock_port_name, const std::string&, m_clk_port_name);
		ClockPort* get_clock_port() const;

		HierObject* instantiate() override;

	protected:
		std::string m_clk_port_name;
	};
}