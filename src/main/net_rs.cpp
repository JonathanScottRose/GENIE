#include "genie/genie.h"
#include "genie/net_topo.h"
#include "net_rs.h"

using namespace genie;

namespace
{
	// Define network
	class NetRS : public Network
	{
	public:
		NetRS(NetType id)
			: Network(id)
		{
			m_name = "rs";
			m_desc = "Routed Streaming";
			m_src_multibind = true;
			m_sink_multibind = true;

			RSPort::ROLE_READY = add_sig_role(SigRole("ready", SigRole::REV, false));
			RSPort::ROLE_VALID = add_sig_role(SigRole("valid", SigRole::FWD, false));
			RSPort::ROLE_DATA = add_sig_role(SigRole("data", SigRole::FWD, true));
			RSPort::ROLE_DATA_BUNDLE = add_sig_role(SigRole("databundle", SigRole::FWD, true));
			RSPort::ROLE_LPID = add_sig_role(SigRole("lpid", SigRole::FWD, false));
			RSPort::ROLE_EOP = add_sig_role(SigRole("eop", SigRole::FWD, false));
			RSPort::ROLE_SOP = add_sig_role(SigRole("sop", SigRole::FWD, false));
		}

		~NetRS()
		{
		}

		Link* create_link()
		{
			auto result = new RSLink();
			result->asp_add(new ALinkContainment());
			return result;
		}

		Endpoint* create_endpoint(Dir dir)
		{
			return new RSEndpoint(dir);
		}

		Port* create_port(Dir dir)
		{
			return new RSPort(dir);
		}
	};
}

// Register the network type
const NetType genie::NET_RS = Network::add<NetRS>();
SigRoleID genie::RSPort::ROLE_DATA;
SigRoleID genie::RSPort::ROLE_DATA_BUNDLE;
SigRoleID genie::RSPort::ROLE_VALID;
SigRoleID genie::RSPort::ROLE_READY;
SigRoleID genie::RSPort::ROLE_EOP;
SigRoleID genie::RSPort::ROLE_SOP;
SigRoleID genie::RSPort::ROLE_LPID;


//
// RSPort
//

RSPort::RSPort(Dir dir)
: Port(dir, NET_RS)
{
	set_connectable(NET_RS, dir);
}

RSPort::RSPort(Dir dir, const std::string& name)
: RSPort(dir)
{
	set_name(name);
}

RSPort::~RSPort()
{
}

HierObject* RSPort::instantiate()
{
	auto result = new RSPort(*this);

	// Only copy linkpoints upon instantiation
	for (auto& c : get_children_by_type<RSLinkpoint>())
	{
		result->add_child(c->instantiate());
	}

	return result;
}

void RSPort::refine_rvd()
{
	// T
}

void RSPort::refine_topo()
{
	// Validate: if have linkpoints, then can't have any direct connections ourselves
	{
		bool connected = get_endpoint(NET_RS, LinkFace::OUTER)->is_connected();
		connected |= get_endpoint(NET_RS, LinkFace::INNER)->is_connected();
		
		if (get_children_by_type<RSLinkpoint>().size() > 0 && connected)
		{
			throw HierException(this, "cannot both have linkpoints and direct connections");
		}
	}

	// Create a topo subport
	TopoPort* subp = new TopoPort(this->get_dir());
	std::string subp_name = genie::make_reserved_name("topo");
	subp->set_name(subp_name, true);

	add_child(subp);
}

void RSPort::refine(NetType target)
{
	// Pass on to children, for refining to non-topo networks and such
	HierObject::refine(target);

	// no switch() because not constant :(
	if (target == NET_TOPO) refine_topo();
	else if (target == NET_RVD) refine_rvd();
}

TopoPort* RSPort::get_topo_port() const
{
	return as_a<TopoPort*>(get_child(genie::make_reserved_name("topo")));
}

//
// ARSEndpoint
//

RSEndpoint::RSEndpoint(Dir dir)
: Endpoint(NET_RS, dir)
{
}

RSEndpoint::~RSEndpoint()
{
}

RSPort* RSEndpoint::get_phys_rs_port()
{
	RSPort* result = as_a<RSPort*>(get_obj());
	if (!result)
		result = as_a<RSPort*>(get_obj()->get_parent());

	assert(result);
	return result;
}

//
// RSLinkpoint
//

RSLinkpoint::RSLinkpoint(Dir dir, Type type)
	: m_type(type), m_dir(dir)
{
	set_connectable(NET_RS, dir);
}

RSLinkpoint::~RSLinkpoint()
{
}

HierObject* RSLinkpoint::instantiate()
{
	auto result = new RSLinkpoint(get_dir(), get_type());
	result->set_name(get_name());
	return result;
}

namespace
{
	static std::vector<std::pair<RSLinkpoint::Type, const char*>> s_lptype_mapping =
	{
		{ RSLinkpoint::BROADCAST, "BROADCAST" },
		{ RSLinkpoint::UNICAST, "UNICAST" },
		{ RSLinkpoint::MULTICAST, "MULTICAST" }
	};
}

RSLinkpoint::Type RSLinkpoint::type_from_str(const char* str)
{
	Type result;
	bool got = util::str_to_enum(s_lptype_mapping, str, &result);
	if (!got)
		result = INVALID;

	return result;
}

RSPort* RSLinkpoint::get_rs_port() const
{
	return as_a<RSPort*>(get_parent());
}

