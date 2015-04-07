#include "genie/net_reset.h"
#include "genie/genie.h"

using namespace genie;

namespace
{
	// Define network
	class NetReset : public Network
	{
	public:
		NetReset(NetType id)
			: Network(id)
		{
			m_name = "reset";
			m_desc = "Reset";
			m_src_multibind = true;
			m_sink_multibind = false;

			ResetPort::ROLE_RESET = add_sig_role(SigRole("reset", SigRole::FWD));
		}

		~NetReset()
		{
		}

		Port* create_port(Dir dir)
		{
			return new ResetPort(dir);
		}
	};
}

// Register the network type
const NetType genie::NET_RESET = Network::add<NetReset>();
SigRoleID genie::ResetPort::ROLE_RESET;


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
