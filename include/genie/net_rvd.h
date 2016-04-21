#pragma once

#include "genie/networks.h"
#include "genie/structure.h"
#include "genie/protocol.h"

namespace genie
{
	class ClockPort;

    // Backpressure capabilities
    struct RVDBackpressure
    {
        bool configurable;
        enum Status
        {
            UNSET,
            ENABLED,
            DISABLED
        } status;

        RVDBackpressure() : configurable(false), status(UNSET) {}
        void make_configurable() { configurable = true; status = UNSET; }
        void force_enable() { configurable = false; status = ENABLED; }
        void force_disable() { configurable = false; status = DISABLED; }
    };

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
        PROP_GETR(proto, PortProtocol&, m_proto);
        PROP_GETR(bp_status, RVDBackpressure&, m_bp_status);

        ClockPort* get_clock_port() const;
		HierObject* instantiate() override;

	protected:
        RVDBackpressure m_bp_status;
		PortProtocol m_proto;
		std::string m_clk_port_name;
	};

	class RVDLink : public Link
	{
	public:
        RVDLink();

		PROP_GET_SET(latency, int, m_latency);

        Link* clone() const override;

	protected:
		int m_latency = 0;
	};
}