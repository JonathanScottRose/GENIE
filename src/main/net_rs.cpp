#include "genie/genie.h"
#include "genie/net_topo.h"
#include "genie/net_clock.h"
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

		Link* create_link() override
		{
			auto result = new RSLink();
			result->asp_add(new ALinkContainment());
			return result;
		}

		Port* create_port(Dir dir) override
		{
			return new RSPort(dir);
		}

		Port* make_export(System* sys, Port* port, const std::string& name) override
		{
			auto orig_port = as_a<RSPort*>(port);
			assert(orig_port);

			// First do the default actions for creating an export
			RSPort* result = static_cast<RSPort*>(Network::make_export(sys, port, name));

			// If the original port had linkpoints, undo the orig->result Link that was created
			// by make_export, and instead create many Links between the original/exported Linkpoints
			auto lps = orig_port->get_linkpoints();
			if (lps.size() > 0)
			{
				sys->disconnect(orig_port, result);

				for (auto& orig_lp : lps)
				{
					// find the corresponding linkpoint in the exported port, make connection
					auto result_lp = result->get_child_as<RSLinkpoint>(orig_lp->get_name());
					assert(result_lp);

					sys->connect(orig_lp, result_lp, NET_RS);
				}
			}

			// Finally, find the correct associated clock port for this export.
			// Step 1: find the associated clock sink for the original port
			auto orig_clk_sink = orig_port->get_clock_port();
			if (!orig_clk_sink || orig_clk_sink->get_dir() != Dir::IN)
				throw HierException(orig_port, "can't export - no associated clock sink");

			// Step 2: get the thing it's connected to
			auto orig_clk_ep = orig_clk_sink->get_endpoint(NET_CLOCK, LinkFace::OUTER);
			auto result_clk_sink = as_a<ClockPort*>(orig_clk_ep->get_remote_obj0());
			if (!result_clk_sink)
				throw HierException(orig_port, "can't export - associated clock sink is unconnected");

			// Step 3: associate it with our result port
			result->set_clock_port_name(result_clk_sink->get_name());

			return result;
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
		
		if (get_linkpoints().size() > 0 && connected)
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

List<RSLinkpoint*> RSPort::get_linkpoints() const
{
	return get_children_by_type<RSLinkpoint>();
}

ClockPort* RSPort::get_clock_port() const
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

//
// RSLinkpoint
//

RSLinkpoint::RSLinkpoint(Dir dir, Type type)
	: Port(dir, NET_RS), m_type(type)
{
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

