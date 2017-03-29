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

LinkRSLogical::LinkRSLogical()
{
}

LinkRSLogical::LinkRSLogical(const LinkRSLogical& o)
{
}

LinkRSLogical::~LinkRSLogical()
{
}

Link * LinkRSLogical::clone() const
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
// NetRS
//

NetType genie::impl::NET_RS;

void NetRS::init()
{
	NET_RS = impl::register_network(new NetRS);
}

NetRS::NetRS()
{
	m_short_name = "rs";
	m_default_max_conn_in = 1;
	m_default_max_conn_out = 1;
}

Link* NetRS::create_link() const
{
	return new LinkRS();
}

//
// LinkRS
//

LinkRS::LinkRS()
{
}

LinkRS::LinkRS(const LinkRS& o)
{
}

LinkRS::~LinkRS()
{
}

Link * LinkRS::clone() const
{
	return new LinkRS(*this);
}

//
// RS Field network
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
// RS Field link
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