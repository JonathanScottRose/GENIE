#include "genie/net_rvd.h"
#include "genie/net_clock.h"
#include "genie/genie.h"

using namespace genie;

NetType genie::NET_RVD = NET_INVALID;
SigRoleID genie::RVDPort::ROLE_VALID = ROLE_INVALID;
SigRoleID genie::RVDPort::ROLE_READY = ROLE_INVALID;
SigRoleID genie::RVDPort::ROLE_DATA = ROLE_INVALID;
SigRoleID genie::RVDPort::ROLE_DATA_CARRIER = ROLE_INVALID;

void NetRVD::init()
{
    NET_RVD = Network::reg<NetRVD>();
}

NetRVD::NetRVD()
{
	m_name = "rvd";
	m_desc = "Point-to-Point Ready/Valid/Data";
	m_default_max_in = 1;
	m_default_max_out = 1;

    add_sig_role(RVDPort::ROLE_DATA = SigRole::reg("data", SigRole::FWD, true));
    add_sig_role( RVDPort::ROLE_DATA_CARRIER = SigRole::reg("xdata", SigRole::FWD, false));
    add_sig_role(RVDPort::ROLE_VALID = SigRole::reg("valid", SigRole::FWD, false));
    add_sig_role(RVDPort::ROLE_READY = SigRole::reg("ready", SigRole::REV, false));
}

Link* NetRVD::create_link()
{
    return new RVDLink();
}

Port* NetRVD::create_port(Dir dir)
{
	return new RVDPort(dir);
}


//
// RVDPort
//

RVDPort::RVDPort(Dir dir)
: Port(dir, NET_RVD)
{
    // Allow unlimited _internal_ fanin/fanout
    set_max_links(NET_RVD, dir_rev(dir), Endpoint::UNLIMITED);
    asp_add(new ALinkContainment());
}

RVDPort::RVDPort(Dir dir, const std::string& name)
	: RVDPort(dir)
{
	set_name(name);
}

RVDPort::~RVDPort()
{
}

HierObject* RVDPort::instantiate()
{
	return new RVDPort(*this);
}

ClockPort* RVDPort::get_clock_port() const
{
	// Use associated clock port name, search for a ClockPort named this on the node we're attached
	// to.
	auto node = get_node();
	ClockPort* result = nullptr;
	
	try
	{
		result = as_a<ClockPort*>(node->get_port(get_clock_port_name()));
	}
	catch (HierNotFoundException&)
	{
	}

	return result;
}

RVDLink::RVDLink()
{
    asp_add(new ALinkContainment());
}

Link* RVDLink::clone() const
{
    auto result = new RVDLink(*this);
    result->asp_add(new ALinkContainment());
    return result;
}

int RVDLink::get_width() const
{
    // Get endpoints
    auto src = (RVDPort*)get_src();
    auto sink = (RVDPort*)get_sink();

    return PortProtocol::calc_transmitted_width(src->get_proto(), sink->get_proto());
}
