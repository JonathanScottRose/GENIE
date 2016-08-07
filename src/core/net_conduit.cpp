#include "genie/net_conduit.h"
#include "genie/genie.h"

using namespace genie;

namespace
{
	// Define network
	class NetConduit : public Network
	{
	public:
		NetConduit(NetType id)
			: Network(id)
		{
			m_name = "conduit";
			m_desc = "Point-to-Point Conduit";
			m_default_max_in = 1;
			m_default_max_out = Endpoint::UNLIMITED;

			add_sig_role(ConduitPort::ROLE_FWD);
			add_sig_role(ConduitPort::ROLE_REV);
			add_sig_role(ConduitPort::ROLE_OUT);
			add_sig_role(ConduitPort::ROLE_IN);
			add_sig_role(ConduitPort::ROLE_INOUT);
		}

		Port* create_port(Dir dir) override
		{
			return new ConduitPort(dir);
		}
	};
}

// Register the network type
const NetType genie::NET_CONDUIT = Network::reg<NetConduit>();
const SigRoleID genie::ConduitPort::ROLE_FWD = SigRole::reg("fwd", SigRole::FWD, true);
const SigRoleID genie::ConduitPort::ROLE_REV = SigRole::reg("rev", SigRole::REV, true);
const SigRoleID genie::ConduitPort::ROLE_OUT = SigRole::reg("out", SigRole::OUT, true);
const SigRoleID genie::ConduitPort::ROLE_IN = SigRole::reg("in", SigRole::IN, true);
const SigRoleID genie::ConduitPort::ROLE_INOUT = SigRole::reg("inout", SigRole::INOUT, true);

//
// ConduitPort
//

ConduitPort::ConduitPort(Dir dir)
: Port(dir, NET_CONDUIT)
{
}

ConduitPort::ConduitPort(Dir dir, const std::string& name)
	: ConduitPort(dir)
{
	set_name(name);
}

ConduitPort::~ConduitPort()
{
}

HierObject* ConduitPort::instantiate()
{
	return new ConduitPort(*this);
}

