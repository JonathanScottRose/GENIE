#include "pch.h"
#include "net_rs.h"
#include "port_rs.h"
#include "flow.h"
#include "genie_priv.h"

using namespace genie::impl;

//
// NetRSLogical
//

NetType genie::impl::NET_RS_LOGICAL;

void NetRSLogical::init()
{
	NET_RS_LOGICAL = impl::register_network(new NetRSLogical);
}

NetRSLogical::NetRSLogical()
{
	m_short_name = "rs_logical";
	m_default_max_conn_in = Endpoint::UNLIMITED;
	m_default_max_conn_out = Endpoint::UNLIMITED;
}

Link* NetRSLogical::create_link() const
{
	return new LinkRSLogical();
}

//
// LinkRSLogical
//

unsigned LinkRSLogical::get_src_addr() const
{
	return m_src_addr;
}

unsigned LinkRSLogical::get_sink_addr() const
{
	return m_sink_addr;
}

LinkRSLogical::LinkRSLogical()
	: m_src_addr(genie::LinkRS::ADDR_ANY),
	m_sink_addr(genie::LinkRS::ADDR_ANY),
	m_domain_id(flow::DomainRS::INVALID)
{
}

LinkRSLogical::LinkRSLogical(const LinkRSLogical& o)
	: Link(o), m_src_addr(o.m_src_addr), m_sink_addr(o.m_sink_addr),
	m_domain_id(o.m_domain_id)
{
}

LinkRSLogical::~LinkRSLogical()
{
}

LinkRSLogical * LinkRSLogical::clone() const
{
	return new LinkRSLogical(*this);
}

//
// NetRSPhys
//

NetType genie::impl::NET_RS_PHYS;

void NetRSPhys::init()
{
	NET_RS_PHYS = impl::register_network(new NetRSPhys);
}

NetRSPhys::NetRSPhys()
{
	m_short_name = "rs_physical";
	m_default_max_conn_in = 1;
	m_default_max_conn_out = 1;
}

Link* NetRSPhys::create_link() const
{
	return new LinkRSPhys();
}

//
// LinkRSPhys
//

LinkRSPhys::LinkRSPhys()
	: m_latency(0)
{
}

LinkRSPhys::~LinkRSPhys()
{
}

Link * LinkRSPhys::clone() const
{
	return new LinkRSPhys(*this);
}

//
// RS FieldInst network
//

NetType genie::impl::NET_RS_SUB;

void NetRSSub::init()
{
	NET_RS_SUB = impl::register_network(new NetRSSub);
}

NetRSSub::NetRSSub()
{
	m_short_name = "rs_field";
	m_default_max_conn_in = 1;
	m_default_max_conn_out = 1;
}

Link* NetRSSub::create_link() const
{
	return new LinkRSSub();
}

//
// RS FieldInst link
//

LinkRSSub::LinkRSSub()
{
}

LinkRSSub::LinkRSSub(const LinkRSSub& o)
	: Link(o)
{
}

LinkRSSub::~LinkRSSub()
{
}

Link * LinkRSSub::clone() const
{
	return new LinkRSSub(*this);
}