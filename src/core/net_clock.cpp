#include "genie/net_clock.h"
#include "genie/genie.h"

using namespace genie;

namespace
{
	// Define network
	class NetClock : public Network
	{
	public:
		NetClock(NetType id)
			: Network(id)
		{
			m_name = "clock";
			m_desc = "Clock";
			m_src_multibind = true;
			m_sink_multibind = false;

			ClockPort::ROLE_CLOCK = add_sig_role(SigRole("clock", SigRole::FWD));
		}

		~NetClock()
		{
		}

		Port* create_port(Dir dir)
		{
			return new ClockPort(dir);
		}
	};
}

// Register the network type
const NetType genie::NET_CLOCK = Network::add<NetClock>();
SigRoleID genie::ClockPort::ROLE_CLOCK;


//
// ClockPort
//

ClockPort::ClockPort(Dir dir)
: Port(dir, NET_CLOCK)
{
	set_connectable(NET_CLOCK, dir);
}

ClockPort::ClockPort(Dir dir, const std::string& name)
	: ClockPort(dir)
{
	set_name(name);
}

ClockPort::~ClockPort()
{
}

HierObject* ClockPort::instantiate()
{
	return new ClockPort(*this);
}


