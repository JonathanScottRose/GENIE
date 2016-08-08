#pragma once

#include "genie/networks.h"
#include "genie/structure.h"

namespace genie
{
	// Globally-accessible network type ID. Maybe move to a static member?
	extern NetType NET_CLOCK;

    // Define network
    class NetClock : public Network
    {
    public:
        static void init();
        NetClock();
        Port* create_port(Dir dir) override;
    };

	class ClockPort : public Port
	{
	public:
		static SigRoleID ROLE_CLOCK;

		ClockPort(Dir dir);
		ClockPort(Dir dir, const std::string& name);
		~ClockPort();

		HierObject* instantiate() override;

		ClockPort* get_clock_driver() const;
	};
}