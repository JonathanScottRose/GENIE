#pragma once

#include "genie/networks.h"
#include "genie/structure.h"

namespace genie
{
	// Globally-accessible network type ID. Maybe move to a static member?
	extern const NetType NET_CLOCK;

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