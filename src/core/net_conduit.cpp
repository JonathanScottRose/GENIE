#include "pch.h"
#include "net_conduit.h"
#include "genie_priv.h"

using namespace genie::impl;

//
// Conduit
//

NetType genie::impl::NET_CONDUIT;

void NetConduit::init()
{
	NET_CONDUIT = impl::register_network(new NetConduit);
}

NetConduit::NetConduit()
{
	m_short_name = "conduit";
	m_default_max_conn_in = 1;
	m_default_max_conn_out = 1;
}

Link* NetConduit::create_link() const
{
	return new LinkConduit();
}

//
// Conduit Sub
//

NetType genie::impl::NET_CONDUIT_SUB;

void NetConduitSub::init()
{
	NET_CONDUIT_SUB = impl::register_network(new NetConduitSub);
}

NetConduitSub::NetConduitSub()
{
	m_short_name = "conduit_sub";
	m_default_max_conn_in = 1;
	m_default_max_conn_out = Endpoint::UNLIMITED;
}

Link* NetConduitSub::create_link() const
{
	return new LinkConduitSub();
}