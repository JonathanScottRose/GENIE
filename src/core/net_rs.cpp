#include "pch.h"
#include "net_rs.h"
#include "port_rs.h"
#include "flow.h"
#include "genie_priv.h"

using namespace genie::impl;
using genie::AddressVal;

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

AddressVal LinkRSLogical::get_src_addr() const
{
	return m_src_addr;
}

AddressVal LinkRSLogical::get_sink_addr() const
{
	return m_sink_addr;
}

void LinkRSLogical::set_packet_size(unsigned size)
{
	m_pkt_size = size;
}

void LinkRSLogical::set_importance(float imp)
{
	if (imp < 0 || imp > 1)
		throw Exception("Importance must be between 0 and 1");

	m_importance = imp;
}

LinkRSLogical::LinkRSLogical()
	: m_src_addr(genie::LinkRS::ADDR_ANY),
	m_sink_addr(genie::LinkRS::ADDR_ANY),
	m_domain_id(flow::DomainRS::INVALID),
	m_pkt_size(PKT_SIZE_UNSET),
	m_importance(IMPORTANCE_UNSET)
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
	: m_lat_or_comb(0)
{
}

LinkRSPhys::~LinkRSPhys()
{
}

Link * LinkRSPhys::clone() const
{
	return new LinkRSPhys(*this);
}

unsigned LinkRSPhys::get_latency() const
{
	return (unsigned)std::max(0, m_lat_or_comb);
}

void LinkRSPhys::set_latency(unsigned latency)
{
	m_lat_or_comb = (int)latency;
}

unsigned LinkRSPhys::get_logic_depth() const
{
	return (unsigned)std::max(0, -m_lat_or_comb);
}

void LinkRSPhys::set_logic_depth(unsigned depth)
{
	m_lat_or_comb = -(int)depth;
}

