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

RoleBinding* ConduitPort::get_matching_role_binding(RoleBinding* other)
{
	// With ConduitPorts, the following connectivity is possible:
	// FWD->FWD
	// REV->REV
	// INOUT->INOUT
	// IN->OUT
	// OUT->IN

	SigRoleID matching_id;
	SigRoleID other_id = other->get_id();
	const std::string& tag = other->get_tag();

	if (other_id == ConduitPort::ROLE_FWD) matching_id = ConduitPort::ROLE_FWD;
	else if (other_id == ConduitPort::ROLE_REV) matching_id = ConduitPort::ROLE_REV;
	else if (other_id == ConduitPort::ROLE_IN) matching_id = ConduitPort::ROLE_OUT;
	else if (other_id == ConduitPort::ROLE_OUT) matching_id = ConduitPort::ROLE_IN;
	else if (other_id == ConduitPort::ROLE_INOUT) matching_id = ConduitPort::ROLE_INOUT;
	else assert(false);

	return get_role_binding(matching_id, tag);
}