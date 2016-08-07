#include "genie/node_split.h"
#include "genie/net_rvd.h"
#include "genie/net_clock.h"
#include "genie/net_reset.h"
#include "genie/net_topo.h"
#include "genie/genie.h"
#include "genie/vlog_bind.h"
#include "genie/net_rs.h"

using namespace genie;
using namespace vlog;

const FieldID NodeSplit::FIELD_FLOWID = Field::reg();

namespace
{
	const std::string MODULE = "genie_split";

	const std::string INPORT_NAME = "in";
	const std::string OUTPORT_NAME = "out";
	const std::string CLOCKPORT_NAME = "clock";
	const std::string RESETPORT_NAME = "reset";
}

void NodeSplit::init_vlog()
{
	auto vinfo = new NodeVlogInfo(MODULE);

	vinfo->add_port(new vlog::Port("clk",  1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("reset",  1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_data",  "WO", vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_flow",  "WF", vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_valid",  1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("o_ready",  1, vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_valid",  "NO", vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_data",  "WO", vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_flow",  "WF", vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("i_ready",  "NO", vlog::Port::IN));

	set_hdl_info(vinfo);
}

NodeSplit::NodeSplit()
{
	init_vlog();

	// Clock and reset ports are straightforward
	auto port = add_port(new ClockPort(Dir::IN, CLOCKPORT_NAME));
	port->add_role_binding(ClockPort::ROLE_CLOCK, new VlogStaticBinding("clk"));

	port = add_port(new ResetPort(Dir::IN, RESETPORT_NAME));
	port->add_role_binding(ResetPort::ROLE_RESET, new VlogStaticBinding("reset"));

	// Input port and output port start out as Topo ports
	auto inport = add_port(new TopoPort(Dir::IN, INPORT_NAME));
	auto outport = add_port(new TopoPort(Dir::OUT, OUTPORT_NAME));

    connect(inport, outport);

	// Output may have multiple connections
	outport->set_max_links(NET_TOPO, Dir::OUT, Endpoint::UNLIMITED);
}

NodeSplit::~NodeSplit()
{
}

TopoPort* NodeSplit::get_topo_input() const
{
	return as_a<TopoPort*>(get_port(INPORT_NAME));
}

TopoPort* NodeSplit::get_topo_output() const
{
	return as_a<TopoPort*>(get_port(OUTPORT_NAME));
}

int NodeSplit::get_n_outputs() const
{
	return get_topo_output()->get_n_rvd_ports();
}

RVDPort* NodeSplit::get_rvd_input() const
{
	return get_topo_input()->get_rvd_port();
}

RVDPort* NodeSplit::get_rvd_output(int idx) const
{
	return get_topo_output()->get_rvd_port(idx);
}

void NodeSplit::refine(NetType target)
{
	// Do the default. TOPO ports will get the correct number of RVD subports.
	HierObject::refine(target);

	if (target == NET_RVD)
	{
		int n_out = get_n_outputs();
		define_param("NO", n_out);

		// Make bindings for the RVD ports
		auto inport = get_rvd_input();
		inport->set_clock_port_name(CLOCKPORT_NAME);
		inport->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("i_valid"));
		inport->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("o_ready"));
		inport->add_role_binding(RVDPort::ROLE_DATA, "flowid", new VlogStaticBinding("i_flow"));
		inport->add_role_binding(RVDPort::ROLE_DATA_CARRIER, new VlogStaticBinding("i_data"));
        inport->get_bp_status().make_configurable();
		inport->get_proto().set_carried_protocol(&m_proto);

		for (int i = 0; i < n_out; i++)
		{
			auto outport = get_rvd_output(i);

			outport->set_clock_port_name(CLOCKPORT_NAME);
			outport->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("o_valid", 1, i));
			outport->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("i_ready", 1, i));
			outport->add_role_binding(RVDPort::ROLE_DATA, "flowid", new VlogStaticBinding("o_flow"));
			outport->add_role_binding(RVDPort::ROLE_DATA_CARRIER, new VlogStaticBinding("o_data"));
            outport->get_bp_status().make_configurable();
			outport->get_proto().set_carried_protocol(&m_proto);

			connect(inport, outport, NET_RVD);
		}
	}
}

HierObject* NodeSplit::instantiate()
{
	throw HierException(this, "not instantiable");
}

void NodeSplit::configure()
{
    RVDPort* inport = get_rvd_input();	      

	int n_outputs = get_n_outputs();
	
    // Route RS Links over internal RVD links created during refinement
    for (int i = 0; i < n_outputs; i++)
    {
        RVDPort* rvd_out = get_rvd_output(i);

        auto rs_links = RSLink::get_from_rvd_port(rvd_out);
        auto int_link = get_links(inport, rvd_out).front();

        for (auto rs_link : rs_links)
            int_link->asp_get<ALinkContainment>()->add_parent_link(rs_link);
    }

    // Get all RS links that travel through the split node's input
    auto in_rs_links = RSLink::get_from_rvd_port(inport);

	// For each Flow ID, find out which split node outputs continue to carry it forward.
	// This map holds which flow IDs go to which outputs. First gather all the unique Flow IDs
	// and initialze the output bool vectors.
	std::map<Value, std::vector<bool>> route_map;
	for (auto i : in_rs_links)
	{
		auto rs_link = (RSLink*)i;

		auto& vec = route_map[rs_link->get_flow_id()];
		vec.resize(n_outputs);
	}

	// Next, iterate through the unique Flow ID keys of the map and find out which outputs
	// have the same RS links with the same Flow IDs.
	for (auto& i : route_map)
	{
		auto& flow_id = i.first;
		auto& vec = i.second;

		// Search each RVD output
		for (int j = 0; j < n_outputs; j++)
		{
			// Get the output port and RVD link
			RVDPort* outport = get_rvd_output(j);
			Link* out_rvd_link = outport->get_endpoint(NET_RVD, LinkFace::OUTER)->get_link0();

			// Unconnected? Treat it as a 'false' entry in the vector and move on
			if (!out_rvd_link)
			{
				vec[j] = false;
				continue;
			}

			// Find if any of the RS links being carried across the RVD link has the Flow ID we want
			auto out_rs_links = out_rvd_link->asp_get<ALinkContainment>()->get_all_parent_links(NET_RS);
			bool match = std::find_if(out_rs_links.begin(), out_rs_links.end(), [&](Link* k)
			{
				auto test_rs = (RSLink*)k;
				return test_rs->get_flow_id() == flow_id;
			}) != out_rs_links.end();

			// Store in the route map
			vec[j] = match;
		}
	}

	// Calculate the flow id width, make sure it's consistent
	int fid_width = -1;
	for (auto& i : route_map)
	{
		auto& fid = i.first;
		if (fid_width >= 0 && fid_width != fid.get_width())
			throw HierException(this, "mismatched Flow ID widths in routing table");
	
		fid_width = fid.get_width();
	}

	// Set node parameters: WF (flow id width), NF (number of entries)
	int n_entries = route_map.size();
	define_param("NF", n_entries);
	define_param("WF", fid_width);

	// Build the FLOWS and ENABLES parameters that define the routing table
	std::string flows_param;
	std::string enables_param;

	int entries_written = n_entries;
	for (auto& i : route_map)
	{
		auto& flow_id = i.first;
		auto& enables_vec = i.second;

		// Binarify the flow id
		flows_param = util::to_binary(flow_id, fid_width) + flows_param;
		
		// Binarify the enables (n_outputs bits in each vector)
		for (int j = 0; j < n_outputs; j++)
			enables_param = (enables_vec[j]? "1": "0") + enables_param;

		// Make the string pretty by separating concatenated binary strings with underscores
		if (--entries_written)
		{
			flows_param = "_" + flows_param;
			enables_param = "_" + enables_param;
		}
	}

	// Finalize the strings
	flows_param = std::to_string(n_entries * fid_width) + "'b" + flows_param;
	enables_param = std::to_string(n_entries * n_outputs) + "'b" + enables_param;

	// Create the parameters as string literals
	define_param("FLOWS", Expression::build_hack_expression(flows_param));
	define_param("ENABLES", Expression::build_hack_expression(enables_param));

	// Add FlowID terminal field to the RVD protocol
	get_rvd_input()->get_proto().add_terminal_field(Field(FIELD_FLOWID, fid_width), "flowid");
	for (int i = 0; i < n_outputs; i++)
	{
		get_rvd_output(i)->get_proto().add_terminal_field(Field(FIELD_FLOWID, fid_width), "flowid");
	}
}

void NodeSplit::do_post_carriage()
{
	// Get data width
	int dwidth = m_proto.get_total_width();

	// Fix data width parameter
	define_param("WO", dwidth);
}

genie::Port* NodeSplit::locate_port(Dir dir, NetType type)
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