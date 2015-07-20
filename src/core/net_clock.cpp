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
			m_default_max_in = 1;
			m_default_max_out = Endpoint::UNLIMITED;

			add_sig_role(ClockPort::ROLE_CLOCK);
		}

		Port* create_port(Dir dir) override
		{
			return new ClockPort(dir);
		}
	};
}

// Register the network type
const NetType genie::NET_CLOCK = Network::reg<NetClock>();
const SigRoleID genie::ClockPort::ROLE_CLOCK = SigRole::reg("clock", SigRole::FWD);


//
// ClockPort
//

ClockPort::ClockPort(Dir dir)
: Port(dir, NET_CLOCK)
{
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

ClockPort* ClockPort::get_clock_driver() const
{
	// Get the driver of this clock port.
	// If we're a sink, find the src that drives us.
	// IF we're a src, then we already are the driver.
	ClockPort* result = nullptr;

	auto ep = get_endpoint_sysface(NET_CLOCK);
	auto dir = ep->get_dir();

	switch (dir)
	{
	case Dir::IN:
		result = (ClockPort*)ep->get_remote_obj0();
		break;
	case Dir::OUT:
		result = const_cast<ClockPort*>(this);
		break;
	default:
		assert(false);
	}

	return result;
}