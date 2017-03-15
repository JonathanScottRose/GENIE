#include "pch.h"
#include "net_rs.h"
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

NetType genie::impl::NET_RS_FIELD;

void NetRSField::init()
{
	NET_RS_FIELD = impl::register_network(new NetRSField);
}

NetRSField::NetRSField()
{
	m_short_name = "rs_field";
	m_default_max_conn_in = 1;
	m_default_max_conn_out = 1;
}

Link* NetRSField::create_link() const
{
	return new LinkRSField();
}

//
// RS Field link
//

LinkRSField::LinkRSField()
{
}

LinkRSField::LinkRSField(const LinkRSField& o)
{
}

LinkRSField::~LinkRSField()
{
}

Link * LinkRSField::clone() const
{
	return new LinkRSField(*this);
}