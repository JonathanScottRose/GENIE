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
			m_src_multibind = false;
			m_sink_multibind = false;

			RVDPort::ROLE_READY = add_sig_role(SigRole("ready", SigRole::REV, false));
			RVDPort::ROLE_VALID = add_sig_role(SigRole("valid", SigRole::FWD, false));
			RVDPort::ROLE_DATA = add_sig_role(SigRole("data", SigRole::FWD, true));
			RVDPort::ROLE_DATA_CARRIER = add_sig_role(SigRole("xdata", SigRole::FWD, false));
		}

		~NetRVD()
		{
		}

		Link* create_link()
		{
			auto result = new Link();
			result->asp_add(new ALinkContainment());
			return result;
		}

		Port* create_port(Dir dir)
		{
			return new RVDPort(dir);
		}
	};
}

// Register the network type
const NetType genie::NET_RVD = Network::add<NetRVD>();
SigRoleID genie::RVDPort::ROLE_DATA;
SigRoleID genie::RVDPort::ROLE_DATA_CARRIER;
SigRoleID genie::RVDPort::ROLE_VALID;
SigRoleID genie::RVDPort::ROLE_READY;

//
// RVDPort
//

RVDPort::RVDPort(Dir dir)
: Port(dir, NET_RVD)
{
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

