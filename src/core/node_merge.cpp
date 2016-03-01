#include "genie/node_merge.h"
#include "genie/net_rvd.h"
#include "genie/net_clock.h"
#include "genie/net_reset.h"
#include "genie/net_topo.h"
#include "genie/genie.h"
#include "genie/vlog_bind.h"
#include "genie/graph.h"
#include "genie/net_rs.h"

using namespace genie;
using namespace vlog;

const FieldID NodeMerge::FIELD_EOP = Field::reg();

namespace
{
	const std::string MODULE = "genie_merge";
	const std::string MODULE_EX = "genie_merge_ex";

	const std::string INPORT_NAME = "in";
	const std::string OUTPORT_NAME = "out";
	const std::string CLOCKPORT_NAME = "clock";
	const std::string RESETPORT_NAME = "reset";
}

void NodeMerge::init_vlog()
{
	using namespace vlog;

	auto vinfo = new NodeVlogInfo(MODULE);

	vinfo->add_port(new vlog::Port("clk", 1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("reset", 1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_data", "NI*WIDTH", vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_valid", "NI", vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_eop", "NI", vlog::Port::IN));
	vinfo->add_port(new vlog::Port("o_ready", "NI", vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_valid", 1, vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_eop", 1, vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_data", "WIDTH", vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("i_ready", 1, vlog::Port::IN));

	set_hdl_info(vinfo);
}

NodeMerge::NodeMerge()
{	
	init_vlog();

	// Clock and reset ports are straightforward
	auto port = add_port(new ClockPort(Dir::IN, CLOCKPORT_NAME));
	port->add_role_binding(ClockPort::ROLE_CLOCK, new VlogStaticBinding("clk"));

	port = add_port(new ResetPort(Dir::IN, RESETPORT_NAME));
	port->add_role_binding(ResetPort::ROLE_RESET, new VlogStaticBinding("reset"));

	// Input port and output port start out as Topo ports
	port = add_port(new TopoPort(Dir::IN, INPORT_NAME));
	add_port(new TopoPort(Dir::OUT, OUTPORT_NAME));

	// input topo port can have multiple connections
	port->set_max_links(NET_TOPO, Dir::IN, Endpoint::UNLIMITED);
}

NodeMerge::~NodeMerge()
{
}

TopoPort* NodeMerge::get_topo_input() const
{
	return as_a<TopoPort*>(get_port(INPORT_NAME));
}

TopoPort* NodeMerge::get_topo_output() const
{
	return as_a<TopoPort*>(get_port(OUTPORT_NAME));
}

int NodeMerge::get_n_inputs() const
{
	return get_topo_input()->get_n_rvd_ports();
}

RVDPort* NodeMerge::get_rvd_output() const
{
	return get_topo_output()->get_rvd_port();
}

RVDPort* NodeMerge::get_rvd_input(int idx) const
{
	return get_topo_input()->get_rvd_port(idx);
}

void NodeMerge::refine(NetType target)
{
	// Default
	HierObject::refine(target);

	if (target == NET_RVD)
	{
		// Make bindings for the RVD ports
		auto outport = get_rvd_output();
		outport->set_clock_port_name(CLOCKPORT_NAME);
		outport->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("o_valid"));
		outport->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("i_ready"));
		outport->add_role_binding(RVDPort::ROLE_DATA, "eop", new VlogStaticBinding("o_eop"));
		outport->add_role_binding(RVDPort::ROLE_DATA_CARRIER, new VlogStaticBinding("o_data"));
		outport->get_proto().add_terminal_field(Field(FIELD_EOP, 1), "eop");
		outport->get_proto().set_carried_protocol(&m_proto);
	
		int n = get_n_inputs();
		define_param("NI", n);

		for (int i = 0; i < get_n_inputs(); i++)
		{
			auto inport = get_rvd_input(i);

			inport->set_clock_port_name(CLOCKPORT_NAME);
			inport->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("i_valid", 1, i));
			inport->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("o_ready", 1, i));
			inport->add_role_binding(RVDPort::ROLE_DATA, "eop", new VlogStaticBinding("i_eop", 1, i));
			inport->add_role_binding(RVDPort::ROLE_DATA_CARRIER, nullptr);
			inport->get_proto().add_terminal_field(Field(FIELD_EOP, 1), "eop");
			inport->get_proto().set_carried_protocol(&m_proto);

			connect(inport, outport, NET_RVD_INTERNAL);
		}
	}
}

HierObject* NodeMerge::instantiate()
{
	throw HierException(this, "node not instantiable");
}

void NodeMerge::do_post_carriage()
{
	// Get data width
	int dwidth = m_proto.get_total_width();

	// Fix parameter
	define_param("WIDTH", dwidth);

	// Update DATA_CARRIER role bindings of each input port
	int n_inputs = get_n_inputs();
	for (int i = 0; i < n_inputs; i++)
	{
		auto port = get_rvd_input(i);
		auto rb = port->get_role_binding(RVDPort::ROLE_DATA_CARRIER);
		rb->set_hdl_binding(new VlogStaticBinding("i_data", dwidth, i*dwidth));
	}
}

void NodeMerge::do_exclusion_check()
{
	using namespace graphs;

	int n_inputs = get_n_inputs();
	
	// For each input, holds the RS links connected to it
	List<List<RSLink*>> links(n_inputs);

	// Vertices represent RSLinks, edges represent possible temporal non-exclusivity
	Graph G;
	RVAttr<RSLink*> link_to_v;

	// Gather all RS Links at every input port, create vertices for them
	for (int i = 0; i < n_inputs; i++)
	{
		RVDPort* port = get_rvd_input(i);
		List<RSLink*>& links_at_port = links[i];

		// Populate list for this port
		links_at_port = RSLink::get_from_rvd_port(port);

		// Create vertices
		for (auto link : links_at_port)
		{
			VertexID v = G.newv();
			link_to_v[link] = v;
		}
	}

	// Create edges based on exclusivity groups
	for (const auto& links_at_port : links)
	{
		for (auto link1 : links_at_port)
		{
			VertexID v1 = link_to_v[link1];

			auto asp = link1->asp_get<ARSExclusionGroup>();
			if (!asp)
				continue;

			for (auto link2 : asp->get_others())
			{
				// We only care about exclusion group entries that refer to links we know about
				if (!link_to_v.count(link2))
					continue;

				VertexID v2 = link_to_v[link2];
				assert(v1 != v2);

				if (!G.hase(v1, v2))
					G.newe(v1, v2);
			}
		}
	}

	// Complement the graph (having an edge now means POSSIBLY NOT EXCLUSIVE)
	G.complement();

	// Collapse all vertices belonging to each port. This leaves one vertex representing all the
	// links at each port, and edges between the ports representing which ports are possibly non-exclusive.
	for (int i = 0; i < n_inputs; i++)
	{
		const List<RSLink*>& links_at_port = links[i];
		if (links_at_port.empty())
			continue;

		VertexID v0 = link_to_v[links_at_port[0]];
		for (unsigned j = 1; j < links_at_port.size(); j++)
		{
			VertexID v = link_to_v[links_at_port[j]];
			G.mergev(v, v0);
		}
	}

	// Count connected components. If this equals the number of ports, then all ports are
	// mutually exclusive.
	int comps = connected_comp(G, nullptr, nullptr);
	bool exclusive = comps == n_inputs;

	// Set the type of merge node based on this result
	if (exclusive)
		((NodeVlogInfo*)get_hdl_info())->set_module_name(MODULE_EX);
}

genie::Port* NodeMerge::locate_port(Dir dir, NetType type)
{
	// We accept TOPO connections directed at the node itself. This resolves
	// to either the input or output TOPO ports depending on dir.
	if (type != NET_INVALID && type != NET_TOPO)
		return HierObject::locate_port(dir, type);

	if (dir == Dir::IN) return get_topo_input();
	else if (dir == Dir::OUT) return get_topo_output();
	else
	{
		assert(false);
		return nullptr;
	}
}