#include "genie/net_rvd.h"
#include "genie/net_clock.h"
#include "genie/genie.h"

using namespace genie;

namespace
{
	// Define network
	class NetRVD : public Network
	{
	public:
		NetRVD(NetType id)
			: Network(id)
		{
			m_name = "rvd";
			m_desc = "Point-to-Point Ready/Valid/Data";
			m_default_max_in = 1;
			m_default_max_out = 1;

			add_sig_role(RVDPort::ROLE_READY);
			add_sig_role(RVDPort::ROLE_VALID);
			add_sig_role(RVDPort::ROLE_DATA);
			add_sig_role(RVDPort::ROLE_DATA_CARRIER);
		}

		Link* create_link() override
		{
			auto result = new Link();
			result->asp_add(new ALinkContainment());
			return result;
		}

		Port* create_port(Dir dir) override
		{
			return new RVDPort(dir);
		}
	};

	class NetRVDInternal : public Network
	{
	public:
		NetRVDInternal(NetType id)
			: Network(id)
		{
			m_name = "rvd_internal";
			m_desc = "Represents RVD connectivity internal to Nodes";
			m_default_max_in =  Endpoint::UNLIMITED;
			m_default_max_out = Endpoint::UNLIMITED;
		}

		Port* create_port(Dir dir) override
		{
			throw Exception("don't do that.");
		}

		Link* create_link() override
		{
			return new RVDInternalLink();
		}
	};
}

// Register the network type
const NetType genie::NET_RVD = Network::reg<NetRVD>();
const NetType genie::NET_RVD_INTERNAL = Network::reg<NetRVDInternal>();
const SigRoleID genie::RVDPort::ROLE_DATA = SigRole::reg("data", SigRole::FWD, true);
const SigRoleID genie::RVDPort::ROLE_DATA_CARRIER = SigRole::reg("xdata", SigRole::FWD, false);
const SigRoleID genie::RVDPort::ROLE_VALID = SigRole::reg("valid", SigRole::FWD, false);
const SigRoleID genie::RVDPort::ROLE_READY = SigRole::reg("ready", SigRole::REV, false);

//
// RVDPort
//

RVDPort::RVDPort(Dir dir)
: Port(dir, NET_RVD)
{
	set_connectable(NET_RVD_INTERNAL, dir);
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

