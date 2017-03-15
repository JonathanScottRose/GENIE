#include "pch.h"
#include "net_clockreset.h"
#include "genie_priv.h"

using namespace genie::impl;

//
// Clock
//

NetType genie::impl::NET_CLOCK;

void NetClock::init()
{
	NET_CLOCK = impl::register_network(new NetClock);
}

NetClock::NetClock()
{
	m_short_name = "clock";
	m_default_max_conn_in = 1;
	m_default_max_conn_out = Endpoint::UNLIMITED;
}

Link* NetClock::create_link() const
{
	return new LinkClock();
}

//
// Reset
//

NetType genie::impl::NET_RESET;

void NetReset::init()
{
	NET_RESET = impl::register_network(new NetReset);
}

NetReset::NetReset()
{
	m_short_name = "reset";
	m_default_max_conn_in = 1;
	m_default_max_conn_out = Endpoint::UNLIMITED;
}

Link* NetReset::create_link() const
{
	return new LinkReset();
}