#include "pch.h"
#include "net_internal.h"
#include "genie_priv.h"

using namespace genie::impl;

//
// NetInternal
//

NetType genie::impl::NET_INTERNAL;

void NetInternal::init()
{
	NET_INTERNAL = impl::register_network(new NetInternal);
}

NetInternal::NetInternal()
{
	m_short_name = "internal";
	m_default_max_conn_in = Endpoint::UNLIMITED;
	m_default_max_conn_out = Endpoint::UNLIMITED;
}

Link* NetInternal::create_link() const
{
	return new LinkInternal();
}

//
// LinkInternal
//

LinkInternal::LinkInternal()
	: m_latency(0)
{
}

LinkInternal::LinkInternal(const LinkInternal& o)
	: m_latency(o.m_latency)
{
}

LinkInternal::~LinkInternal()
{
}

Link * LinkInternal::clone() const
{
	return new LinkInternal(*this);
}
