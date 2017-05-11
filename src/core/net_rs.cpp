#include "pch.h"
#include "net_rs.h"
#include "port_rs.h"
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
	m_sink_addr(genie::LinkRS::ADDR_ANY)
{
}

LinkRSLogical::LinkRSLogical(const LinkRSLogical& o)
	: m_src_addr(o.m_src_addr), m_sink_addr(o.m_sink_addr)
{
}

LinkRSLogical::~LinkRSLogical()
{
}

LinkRSLogical * LinkRSLogical::clone() const
{
	return new LinkRSLogical(*this);
}

unsigned LinkRSLogical::get_domain_id() const
{
	auto src = dynamic_cast<PortRS*>(get_src());
	auto sink = dynamic_cast<PortRS*>(get_sink());

	if (src && sink)
	{
		auto src_id = src->get_domain_id();
		auto sink_id = sink->get_domain_id();
		assert(src_id == sink_id);
		return src_id;
	}

	return 0;
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
{
}

LinkRSPhys::LinkRSPhys(const LinkRSPhys& o)
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
{
}

LinkRSSub::~LinkRSSub()
{
}

Link * LinkRSSub::clone() const
{
	return new LinkRSSub(*this);
}