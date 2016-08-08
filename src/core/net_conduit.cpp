#include "genie/net_conduit.h"
#include "genie/genie.h"

using namespace genie;

NetType genie::NET_CONDUIT = NET_INVALID;
SigRoleID genie::ConduitPort::ROLE_FWD = ROLE_INVALID;
SigRoleID genie::ConduitPort::ROLE_REV = ROLE_INVALID;
SigRoleID genie::ConduitPort::ROLE_IN = ROLE_INVALID;
SigRoleID genie::ConduitPort::ROLE_OUT = ROLE_INVALID;
SigRoleID genie::ConduitPort::ROLE_INOUT = ROLE_INVALID;

void NetConduit::init()
{
    // Register the network type
    NET_CONDUIT = Network::reg<NetConduit>();
}

NetConduit::NetConduit()
{
    m_name = "conduit";
    m_desc = "Point-to-Point Conduit";
    m_default_max_in = 1;
    m_default_max_out = Endpoint::UNLIMITED;

    add_sig_role(ConduitPort::ROLE_FWD = SigRole::reg("fwd", SigRole::FWD, true));
    add_sig_role(ConduitPort::ROLE_REV = SigRole::reg("rev", SigRole::REV, true));
    add_sig_role(ConduitPort::ROLE_OUT = SigRole::reg("out", SigRole::OUT, true));
    add_sig_role(ConduitPort::ROLE_IN = SigRole::reg("in", SigRole::IN, true));
    add_sig_role(ConduitPort::ROLE_INOUT = SigRole::reg("inout", SigRole::INOUT, true));
}

Port* NetConduit::create_port(Dir dir) 		
{
    return new ConduitPort(dir);
}

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

