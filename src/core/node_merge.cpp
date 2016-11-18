#include "genie/node_merge.h"
#include "genie/net_rvd.h"
#include "genie/net_clock.h"
#include "genie/net_reset.h"
#include "genie/net_topo.h"
#include "genie/genie.h"
#include "genie/graph.h"
#include "genie/net_rs.h"

using namespace genie;
using namespace hdl;

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
	using namespace hdl;

	auto vinfo = new NodeHDLInfo(MODULE);

	vinfo->add_port(new hdl::Port("clk", 1, hdl::Port::IN));
	vinfo->add_port(new hdl::Port("reset", 1, hdl::Port::IN));
	vinfo->add_port(new hdl::Port("i_data", "NI*WIDTH", hdl::Port::IN));
	vinfo->add_port(new hdl::Port("i_valid", "NI", hdl::Port::IN));
	vinfo->add_port(new hdl::Port("i_eop", "NI", hdl::Port::IN));
	vinfo->add_port(new hdl::Port("o_ready", "NI", hdl::Port::OUT));
	vinfo->add_port(new hdl::Port("o_valid", 1, hdl::Port::OUT));
	vinfo->add_port(new hdl::Port("o_eop", 1, hdl::Port::OUT));
	vinfo->add_port(new hdl::Port("o_data", "WIDTH", hdl::Port::OUT));
	vinfo->add_port(new hdl::Port("i_ready", 1, hdl::Port::IN));

	set_hdl_info(vinfo);
}

NodeMerge::NodeMerge()
    : m_is_exclusive(false)
{	
	init_vlog();

	// Clock and reset ports are straightforward
	auto port = add_port(new ClockPort(Dir::IN, CLOCKPORT_NAME));
	port->add_role_binding(ClockPort::ROLE_CLOCK, new HDLBinding("clk"));

	port = add_port(new ResetPort(Dir::IN, RESETPORT_NAME));
	port->add_role_binding(ResetPort::ROLE_RESET, new HDLBinding("reset"));

	// Input port and output port start out as Topo ports
	auto inport = add_port(new TopoPort(Dir::IN, INPORT_NAME));
	auto outport = add_port(new TopoPort(Dir::OUT, OUTPORT_NAME));

    // input topo port feeds output topo port with an internal link
    connect(inport, outport);

	// input topo port can have multiple connections (to outside world)
	inport->set_max_links(NET_TOPO, Dir::IN, Endpoint::UNLIMITED);
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
        int n = get_n_inputs();
        define_param("NI", n);

		// Make bindings for the RVD ports
		auto outport = get_rvd_output();
        outport->set_max_links(NET_RVD, Dir::IN, n);
		outport->set_clock_port_name(CLOCKPORT_NAME);
		outport->add_role_binding(RVDPort::ROLE_VALID, new HDLBinding("o_valid"));
        outport->add_role_binding(RVDPort::ROLE_READY, new HDLBinding("i_ready"));
		outport->add_role_binding(RVDPort::ROLE_DATA, "eop", new HDLBinding("o_eop"));
		outport->add_role_binding(RVDPort::ROLE_DATA_CARRIER, new HDLBinding("o_data"));
		outport->get_proto().add_terminal_field(Field(FIELD_EOP, 1), "eop");
        outport->get_bp_status().make_configurable();
		outport->get_proto().set_carried_protocol(&m_proto);

		for (int i = 0; i < n; i++)
		{
			auto inport = get_rvd_input(i);

			inport->set_clock_port_name(CLOCKPORT_NAME);
			inport->add_role_binding(RVDPort::ROLE_VALID, new HDLBinding("i_valid", 1, i));
            inport->add_role_binding(RVDPort::ROLE_READY, new HDLBinding("o_ready", 1, i));
			inport->add_role_binding(RVDPort::ROLE_DATA, "eop", new HDLBinding("i_eop", 1, i));
			inport->add_role_binding(RVDPort::ROLE_DATA_CARRIER, nullptr);
			inport->get_proto().add_terminal_field(Field(FIELD_EOP, 1), "eop");
            inport->get_bp_status().force_enable();
			inport->get_proto().set_carried_protocol(&m_proto);

			connect(inport, outport, NET_RVD);
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
		rb->set_hdl_binding(new HDLBinding("i_data", dwidth, i*dwidth));
	}
}

void NodeMerge::configure()
{
    // Route RS Links over internal RVD links created during refinement
    RVDPort* rvd_out = get_rvd_output();
    for (int i = 0; i < get_n_inputs(); i++)
    {
        RVDPort* rvd_in = get_rvd_input(i);

        auto rs_links = RSLink::get_from_rvd_port(rvd_in);
        auto int_link = (RVDLink*)get_links(rvd_in, rvd_out).front();
        
        for (auto rs_link : rs_links)
            int_link->asp_get<ALinkContainment>()->add_parent_link(rs_link);
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
            assert(link_to_v.count(link) == 0);
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
	m_is_exclusive = comps == n_inputs;

	if (m_is_exclusive)
    {
        // Set the type of merge node based on this result
		get_hdl_info()->set_module_name(MODULE_EX);

        // Loosen backpressure restrictions on inputs
        for (int i = 0; i < n_inputs; i++)
        {
            auto in = get_rvd_input(i);
            in->get_bp_status().make_configurable();
        }
    }
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

AreaMetrics genie::NodeMerge::get_area_usage() const
{
    AreaMetrics result;

    const unsigned lutsize = genie::arch_params().lutsize;

    unsigned n_inputs = get_n_inputs();
    unsigned data_bits = m_proto.get_total_width();
    
    // calculate mux size
    unsigned muxsize = 1;
    while (muxsize + (1<<muxsize) <= lutsize)
        muxsize++;
    muxsize--;

    // get log-base-muxsize of #inputs
    while (n_inputs > 1)
    {
        // ceil(n_inputs/muxsize)
        unsigned luts = (n_inputs + muxsize-1) / muxsize;

        result.luts += luts;

        // Outputs of these luts become inputs of next stage of muxing
        n_inputs = luts;
    }

    result.luts *= data_bits;

    return result;
}
