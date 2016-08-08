#include "genie/net_reset.h"
#include "genie/genie.h"

using namespace genie;

NetType genie::NET_RESET = NET_INVALID;
SigRoleID genie::ResetPort::ROLE_RESET = ROLE_INVALID;

void NetReset::init()
{
    NET_RESET = Network::reg<NetReset>();
}

NetReset::NetReset()
{
    m_name = "reset";
    m_desc = "Reset";
    m_default_max_in = 1;
    m_default_max_out = Endpoint::UNLIMITED;

    add_sig_role(ResetPort::ROLE_RESET = SigRole::reg("reset", SigRole::FWD));
}

Port* NetReset::create_port(Dir dir)
{
    return new ResetPort(dir);
}

//
// ResetPort
//

ResetPort::ResetPort(Dir dir)
: Port(dir, NET_RESET)
{
}

ResetPort::ResetPort(Dir dir, const std::string& name)
	: ResetPort(dir)
{
	set_name(name);
}

ResetPort::~ResetPort()
{
}

HierObject* ResetPort::instantiate()
{
	return new ResetPort(*this);
}