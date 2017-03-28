#include "pch.h"
#include "net_topo.h"
#include "genie_priv.h"

using namespace genie::impl;

//
// NetTopo
//

NetType genie::impl::NET_TOPO;

void NetTopo::init()
{
	NET_TOPO = impl::register_network(new NetTopo);
}

NetTopo::NetTopo()
{
	m_short_name = "topo";
	m_default_max_conn_in = Endpoint::UNLIMITED;
	m_default_max_conn_out = Endpoint::UNLIMITED;
}

Link* NetTopo::create_link() const
{
	return new LinkTopo();
}

//
// LinkTopo
//

void LinkTopo::set_min_regs(unsigned regs)
{
	m_min_regs = regs;
}

LinkTopo::LinkTopo()
	: m_min_regs(0)
{
}

LinkTopo::~LinkTopo()
{
}

Link * LinkTopo::clone() const
{
	return new LinkTopo(*this);
}
