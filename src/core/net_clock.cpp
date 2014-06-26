#include "ct/net_clock.h"
#include "ct/ct.h"

using namespace ct;

//
// Network registration
//

static StaticInit<NetworkDef> s_init([]
{
	ct::get_root()->reg_net_def(new NetClock());
});

//
// Network definition
//

const NetworkID NetClock::ID = "clock";

NetClock::NetClock()
: NetworkDef(NetClock::ID)
{
	m_name = "Clock";

	// Multiple fanout allowed, either by:
	// - source having multiple bound connections
	// - each connection having multiple fanout
	m_src_multibind = true;
	m_sink_multibind = false;
	m_conn_multibind = true;
}

//
// Port definition
//

ClockPortDef::ClockPortDef(const std::string& name, Dir dir)
{
	set_name(name);
	set_dir(dir);
}

ClockPortDef::~ClockPortDef()
{
}

Port* ClockPortDef::instantiate()
{
	return new ClockPort(m_name, this);
}

//
// Port state
//

ClockPort::ClockPort(const std::string& name, ClockPortDef* def)
: Endpoint(this)
{
	set_name(name);
	set_port_def(def);
}

ClockPort::~ClockPort()
{
}

Endpoint* ClockPort::conn_get_default_ep()
{
	// The Port is itself a Clock Network Endpoint
	return this;
}

Dir ClockPort::conn_get_dir() const
{
	return m_port_def->get_dir();
}

const NetworkID& ClockPort::ep_get_net_type() const
{
	return NetClock::ID;
}
