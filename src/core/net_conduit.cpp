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
			m_src_multibind = false;
			m_sink_multibind = false;

			ConduitPort::ROLE_FWD = add_sig_role(SigRole("fwd", SigRole::FWD, true));
			ConduitPort::ROLE_REV = add_sig_role(SigRole("rev", SigRole::REV, true));
			ConduitPort::ROLE_OUT = add_sig_role(SigRole("out", SigRole::OUT, true));
			ConduitPort::ROLE_IN = add_sig_role(SigRole("in", SigRole::IN, true));
			ConduitPort::ROLE_INOUT= add_sig_role(SigRole("inout", SigRole::INOUT, true));
		}

		~NetConduit()
		{
		}

		Port* create_port(Dir dir)
		{
			return new ConduitPort(dir);
		}
	};
}

// Register the network type
const NetType genie::NET_CONDUIT = Network::add<NetConduit>();
SigRoleID genie::ConduitPort::ROLE_FWD;
SigRoleID genie::ConduitPort::ROLE_REV;
SigRoleID genie::ConduitPort::ROLE_OUT;
SigRoleID genie::ConduitPort::ROLE_IN;
SigRoleID genie::ConduitPort::ROLE_INOUT;

//
// ConduitPort
//

ConduitPort::ConduitPort(Dir dir)
: Port(dir, NET_CONDUIT)
{
	set_connectable(NET_CONDUIT, dir);
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
