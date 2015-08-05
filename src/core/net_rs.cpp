#include "genie/genie.h"
#include "genie/net_topo.h"
#include "genie/net_clock.h"
#include "genie/node_merge.h"
#include "genie/net_rs.h"

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
			m_default_max_in =  Endpoint::UNLIMITED;
			m_default_max_out = Endpoint::UNLIMITED;

			add_sig_role(RSPort::ROLE_READY);
			add_sig_role(RSPort::ROLE_VALID);
			add_sig_role(RSPort::ROLE_DATA);
			add_sig_role(RSPort::ROLE_DATA_BUNDLE);
			add_sig_role(RSPort::ROLE_LPID);
			add_sig_role(RSPort::ROLE_EOP);
			add_sig_role(RSPort::ROLE_SOP);
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

		Port* export_port(System* sys, Port* port, const std::string& name) override
		{
			auto orig_port = as_a<RSPort*>(port);
			assert(orig_port);

			// First do the default actions for creating an export
			RSPort* result = static_cast<RSPort*>(Network::export_port(sys, port, name));

			// If the original port had linkpoints, we need to export those as well. This was not done
			// automatically by the generic export_port().
			// Additionally, we'll need to undo the Link made by export_port and instead make links between
			// the individual origina/exported linkpoints.
			auto lps = orig_port->get_linkpoints();
			if (lps.size() > 0)
			{
				sys->disconnect(orig_port, result);

				// For each of the original linkpoints...
				for (auto& orig_lp : lps)
				{
					// Create a copy for the exported RSPort.
					auto result_lp = as_a<RSLinkpoint*>(orig_lp->instantiate());
					result->add_child(result_lp);

					// Connect the original/new linkpoints together
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
const NetType genie::NET_RS = Network::reg<NetRS>();

// Register port signal roles
const SigRoleID RSPort::ROLE_DATA = SigRole::reg("data", SigRole::FWD, false);
const SigRoleID RSPort::ROLE_DATA_BUNDLE = SigRole::reg("databundle", SigRole::FWD, true);
const SigRoleID RSPort::ROLE_VALID = SigRole::reg("valid", SigRole::FWD, false);
const SigRoleID RSPort::ROLE_READY = SigRole::reg("ready", SigRole::REV, false);
const SigRoleID RSPort::ROLE_EOP = SigRole::reg("eop", SigRole::FWD, false);
const SigRoleID RSPort::ROLE_SOP = SigRole::reg("sop", SigRole::FWD, false);
const SigRoleID RSPort::ROLE_LPID = SigRole::reg("lpid", SigRole::FWD, false);

// Register data field types
const FieldID RSPort::FIELD_LPID = Field::reg();
const FieldID RSPort::FIELD_DATA = Field::reg();

//
// RSPort
//

RSPort::RSPort(Dir dir)
: Port(dir, NET_RS), m_domain_id(-1)
{
}

RSPort::RSPort(const RSPort& o)
	: Port(o), m_clk_port_name(o.m_clk_port_name)
{
	// Reset domain membership
	m_domain_id = -1;

	// Only copy linkpoints upon instantiation
	for (auto& c : o.get_children_by_type<RSLinkpoint>())
	{
		add_child(c->instantiate());
	}
}

RSPort::~RSPort()
{
}

HierObject* RSPort::instantiate()
{
	return new RSPort(*this);
}

void RSPort::refine_rvd()
{
	TopoPort* topo_port = get_topo_port();
	if (topo_port->get_n_rvd_ports() == 0)
		return;

	RVDPort* rvd_port = get_topo_port()->get_rvd_port();
	assert(rvd_port);

	// Configure the RVD Port Protocol
	auto& proto = rvd_port->get_proto();

	// Set up role bindings on RVD port, by copying ours
	for (auto& rs_rb : get_role_bindings())
	{
		auto rs_role = rs_rb->get_id();
		auto rs_tag = rs_rb->get_tag();
		const auto rs_rdef = rs_rb->get_role_def();

		// Clone the HDL binding
		auto rvd_hdlb = rs_rb->get_hdl_binding()->clone();

		// Ready/valid get special treatment, everything else is DATA
		if (rs_role == RSPort::ROLE_VALID)
		{
			rvd_port->add_role_binding(RVDPort::ROLE_VALID, rvd_hdlb);
		}
		else if (rs_role == RSPort::ROLE_READY)
		{
			rvd_port->add_role_binding(RVDPort::ROLE_READY, rvd_hdlb);
		}
		else
		{
			// Create the ROLE_DATA with the right tag
			std::string rvd_tag = rs_rdef->get_name();
			if (rs_rdef->get_uses_tags())
				rvd_tag += "_" + rs_tag;

			rvd_port->add_role_binding(RVDPort::ROLE_DATA, rvd_tag, rvd_hdlb);

			// Add a terminal field to the RVD Port's Protocol
			Field field;

			if (rs_role == RSPort::ROLE_EOP)
			{
				field.set_id(NodeMerge::FIELD_EOP);
			}
			else if (rs_role == RSPort::ROLE_LPID)
			{
				field.set_id(RSPort::FIELD_LPID);
			}
			else if (rs_role == RSPort::ROLE_DATA)
			{
				field.set_id(RSPort::FIELD_DATA);
				field.set_domain(this->get_domain_id());
			}
			else if (rs_role == RSPort::ROLE_DATA_BUNDLE)
			{
				field.set_id(RSPort::FIELD_DATA);
				field.set_domain(this->get_domain_id());
				field.set_tag(rs_tag);
			}

			field.set_width(rvd_hdlb->get_width());
			proto.add_terminal_field(field, rvd_tag);
		}
	}

	// Copy associated clock port
	const std::string& clkportname = this->get_clock_port_name();
	if (clkportname.empty())
		throw HierException(this, "missing associated clock port");

	rvd_port->set_clock_port_name(clkportname);
}

void RSPort::refine_topo()
{
	auto linkpoints = get_linkpoints();
	bool has_linkpoints = !linkpoints.empty();
	bool nonlp_connected = get_endpoint_sysface(NET_RS)->is_connected();

	// Validate: if have linkpoints, then can't have any direct connections ourselves
	if (has_linkpoints && nonlp_connected)
	{
		throw HierException(this, "cannot both have linkpoints and direct connections");
	}
	
	// Validate: fixup linkpoint ID encodings to match the width of the likpoint ID port
	if (has_linkpoints)
	{
		auto lp_rb = get_role_binding(ROLE_LPID);
		if (!lp_rb)
		{
			throw HierException(this, "has linkpoints but no lpid signal");
		}

		int lp_rb_width = lp_rb->get_hdl_binding()->get_width();
		for (auto& lp : linkpoints)
		{
			// good place to throw warnings if LPID value is truncated
			auto lpid_val = lp->get_encoding();
			lpid_val.set_width(lp_rb_width);
			lp->set_encoding(lpid_val);
		}
	}

	// Create a topo subport
	TopoPort* subp = new TopoPort(this->get_dir());
	std::string subp_name = genie::hier_make_reserved_name("topo");
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
	return as_a<TopoPort*>(get_child(genie::hier_make_reserved_name("topo")));
}

RVDPort* RSPort::get_rvd_port() const
{
	return get_topo_port()->get_rvd_port();
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

RSPort* RSPort::get_rs_port(Port* p)
{
	// If we're an RSLinkpoint
	RSPort* result = as_a<RSPort*>(p->get_parent());
	 
	// If we're an RSPort
	if (!result) result = as_a<RSPort*>(p);

	// Will return one of the above, or nullptr
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
	return new RSLinkpoint(*this);
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

//
// RSLink
//

RSLink::RSLink()
	: m_domain_id(-1)
{
}

Link* RSLink::clone() const
{
	auto result = new RSLink(*this);
	result->copy_containment(*this);
	return result;
}

List<RSLink*> RSLink::get_from_rvd_port(RVDPort* port)
{
	List<RSLink*> result;

	// Endpoint
	auto ep = port->get_endpoint_sysface(NET_RVD);
	if (!ep)
		return result;

	// RVD Link
	auto rvd_link = ep->get_link0();
	if (!rvd_link)
		return result;

	// Containment
	auto ac = rvd_link->asp_get<ALinkContainment>();
	assert(ac);

	result = util::container_cast<List<RSLink*>>(ac->get_all_parent_links(NET_RS));

	return result;
}

bool RSLink::contained_in_rvd_port(RVDPort* port) const
{
	auto links = get_from_rvd_port(port);
	return util::exists(links, this);
}

//
// ARSExclusionGroup
//

const List<RSLink*>& ARSExclusionGroup::get_others() const
{
	return m_set;
}

void ARSExclusionGroup::add(const List<RSLink*>& list)
{
	for (auto link : list)
	{
		// Never include owning link
		if (link == asp_container())
			continue;

		// Ignore duplicates too
		if (util::exists(m_set, link))
			continue;

		m_set.push_back(link);
	}
}

bool ARSExclusionGroup::has(const RSLink* test) const
{
	return util::exists(m_set, test);		
}

void ARSExclusionGroup::process_and_create(const List<RSLink*>& links)
{
	// Go through each link
	for (auto link : links)
	{
		// Get or create aspect
		auto asp = link->asp_get<ARSExclusionGroup>();
		if (!asp)
			asp = link->asp_add(new ARSExclusionGroup());

		// Add all links to everyone's sets
		asp->add(links);
	}
}

//
// ARSLatencyQueries
//

void ARSLatencyQueries::add(RSLink* link, const std::string& parmname)
{
	for (auto& i : m_queries)
	{
		if (i.second == parmname)
			throw Exception("duplicate latency query parameter name " + parmname);
	}

	m_queries.emplace_back(link, parmname);
}

auto ARSLatencyQueries::queries() const -> const List<Query>&
{
	return m_queries;
}