#include "ct/net_reset.h"
#include "ct/ct.h"

using namespace ct;

//
// Network registration
//

static StaticInit<NetworkDef> s_init([]
{
	ct::get_root()->reg_net_def(new NetReset());
});

//
// Network definition
//

const NetworkID NetReset::ID = "reset";

NetReset::NetReset()
: NetworkDef(NetReset::ID)
{
	m_name = "Reset";

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

ResetPortDef::ResetPortDef(const std::string& name, Dir dir)
{
	set_name(name);
	set_dir(dir);
}

ResetPortDef::~ResetPortDef()
{
}

Port* ResetPortDef::instantiate()
{
	return new ResetPort(m_name, this);
}

//
// Port state
//

ResetPort::ResetPort(const std::string& name, ResetPortDef* def)
: Endpoint(this)
{
	set_name(name);
	set_port_def(def);
}

ResetPort::~ResetPort()
{
}

Endpoint* ResetPort::conn_get_default_ep()
{
	// The Port is itself a Reset Network Endpoint
	return this;
}

Dir ResetPort::conn_get_dir() const
{
	return m_port_def->get_dir();
}

const NetworkID& ResetPort::ep_get_net_type() const
{
	return NetReset::ID;
}
