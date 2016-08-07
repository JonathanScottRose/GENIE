#include <unordered_set>

#include "genie/net_clock.h"
#include "genie/net_reset.h"
#include "genie/net_rvd.h"
#include "genie/vlog_bind.h"
#include "genie/node_flowconv.h"
#include "genie/net_rs.h"
#include "genie/node_split.h"

using namespace genie;
using namespace vlog;

namespace
{
	const std::string MODNAME = "genie_conv";
	const std::string INPORT_NAME = "in";
	const std::string OUTPORT_NAME = "out";
	const std::string CLOCKPORT_NAME = "clk";
	const std::string RESETPORT_NAME = "reset";
}

NodeFlowConv::NodeFlowConv(bool to_flow)
	: Node(), m_to_flow(to_flow)
{
	// Create verilog ports
	auto vinfo = new NodeVlogInfo(MODNAME);
	vinfo->add_port(new vlog::Port("clk", 1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("reset", 1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_data", "WD", vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_field", "WIF", vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_valid", 1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("o_ready", 1, vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_data", "WD", vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_field", "WOF", vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_valid", 1, vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("i_ready", 1, vlog::Port::IN));
	set_hdl_info(vinfo);

	// Create genie ports and port bindings
	auto clkport = add_port(new ClockPort(Dir::IN, CLOCKPORT_NAME));
	clkport->add_role_binding(ClockPort::ROLE_CLOCK, new VlogStaticBinding("clk"));

	auto resetport = add_port(new ResetPort(Dir::IN, RESETPORT_NAME));
	resetport->add_role_binding(ResetPort::ROLE_RESET, new VlogStaticBinding("reset"));

	const std::string& intag = to_flow? "lpid" : "flow_id";
	const std::string& outtag = to_flow? "flow_id" : "lpid";

	auto inport = new RVDPort(Dir::IN, INPORT_NAME);
	inport->set_clock_port_name(CLOCKPORT_NAME);
	inport->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("i_valid"));
    inport->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("o_ready"));
	inport->add_role_binding(RVDPort::ROLE_DATA, intag, new VlogStaticBinding("i_field"));
	inport->add_role_binding(RVDPort::ROLE_DATA_CARRIER, new VlogStaticBinding("i_data"));
    inport->get_bp_status().make_configurable();
	inport->get_proto().set_carried_protocol(&m_proto);
	add_port(inport);

	auto outport = new RVDPort(Dir::OUT, OUTPORT_NAME);
	outport->set_clock_port_name(CLOCKPORT_NAME);
	outport->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("o_valid"));
    outport->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("i_ready"));
	outport->add_role_binding(RVDPort::ROLE_DATA, outtag, new VlogStaticBinding("o_field"));
	outport->add_role_binding(RVDPort::ROLE_DATA_CARRIER, new VlogStaticBinding("o_data"));
    outport->get_bp_status().make_configurable();
	outport->get_proto().set_carried_protocol(&m_proto);
	add_port(outport);

	connect(inport, outport, NET_RVD);
}

void NodeFlowConv::configure()
{
	// Create an lpid<->flow_id mapping, based on the connections at our lp_id-facing side.
	
	// 1) Take the port that has LPID on it (either the input or the output)
	RVDPort* rvd_port = m_to_flow? get_input() : get_output();
	
	// 2) Grab the incoming/outgoing Link on it
	Link* rvd_link = rvd_port->get_endpoint(NET_RVD, LinkFace::OUTER)->get_link0();
	if (!rvd_link)
		throw HierException(rvd_port, "not connected, can't configure flow conversion");

	// 3) Find all the RSLinks it that travel over it
	auto rs_links = rvd_link->asp_get<ALinkContainment>()->get_all_parent_links(NET_RS);

	// 4) Gather all endpoints of these links into a set. Whether to take src or sinks depends
	// on flow converter's direction. All srces (or sinks) should be RSLinkpoints. Keep one
	// exemplar RSLink* associated with each RSLinkpoint* (doesn't matter which)
	std::unordered_map<RSLinkpoint*, RSLink*> lp_to_link;
	for (auto& link : rs_links)
	{
		auto rs_link = as_a<RSLink*>(link);

		Port* ep = m_to_flow? rs_link->get_src() : rs_link->get_sink();
		auto lp = as_a<RSLinkpoint*>(ep);
		if (!lp)
			throw HierException(ep, "not a linkpoint");

		lp_to_link[lp] = rs_link;
	}

	// 5) Create a list of lpid<->flowid or flowid<->lpid depending on conversion direction.
	// Each Linkpoint supplies the lpid.
	// The RSLink supplies the flowid.
	std::vector<std::pair<Value, Value>> mapping;
	for (auto& i : lp_to_link)
	{
		auto& lp = i.first;
		auto& link = i.second;

		const Value& key = m_to_flow? lp->get_encoding() : link->get_flow_id();
		const Value& val = m_to_flow? link->get_flow_id() : lp->get_encoding();

		mapping.emplace_back(key, val);
	}

	// 6) Find the signal widths of the keys/values in mappings and validate to make sure
	// they're all the same
	int key_width = -1;
	int val_width = -1;
	for (auto& i : mapping)
	{
		auto& key = i.first;
		auto& val = i.second;

		if (key_width >= 0 && key_width != key.get_width())
			throw HierException(this, "key width mismatch");

		if (val_width >= 0 && val_width != val.get_width())
			throw HierException(this, "val width mismatch");

		key_width = key.get_width();
		val_width = val.get_width();
	}

	// 7) Set node parameters
	int n_entries = (int)mapping.size();

	define_param("WIF", key_width);
	define_param("WOF", val_width);
	define_param("N_ENTRIES", n_entries);

	// Make a big binary string for keys/vals
	std::string keys_param;
	std::string vals_param;

	int entries_written = n_entries;
	for (auto& i : mapping)
	{
		auto& key = i.first;
		auto& val = i.second;

		keys_param = util::to_binary(key, key_width) + keys_param;
		vals_param = util::to_binary(val, val_width) + vals_param;

		// Make the string pretty by separating concatenated binary strings with underscores
		if (--entries_written)
		{
			keys_param = "_" + keys_param;
			vals_param = "_" + vals_param;
		}
	}

	// Finalize the strings
	keys_param = std::to_string(n_entries * key_width) + "'b" + keys_param;
	vals_param = std::to_string(n_entries * val_width) + "'b" + vals_param;

	// Create the parameters as string literals
	define_param("IF", Expression::build_hack_expression(keys_param));
	define_param("OF", Expression::build_hack_expression(vals_param));

	// Set up RVD protocol
	FieldID key_fid = m_to_flow? RSPort::FIELD_LPID : NodeSplit::FIELD_FLOWID;
	FieldID val_fid = m_to_flow? NodeSplit::FIELD_FLOWID : RSPort::FIELD_LPID;
	const std::string& keytag = m_to_flow? "lpid" : "flow_id";
	const std::string& valtag = m_to_flow? "flow_id" : "lpid";

	get_input()->get_proto().add_terminal_field(Field(key_fid, key_width), keytag);
	get_output()->get_proto().add_terminal_field(Field(val_fid, val_width), valtag);
}

RVDPort* NodeFlowConv::get_input() const
{
	return as_a<RVDPort*>(get_port(INPORT_NAME));
}

RVDPort* NodeFlowConv::get_output() const
{
	return as_a<RVDPort*>(get_port(OUTPORT_NAME));
}

HierObject* NodeFlowConv::instantiate()
{
	throw HierException(this, "node not instantiable");
}

void NodeFlowConv::do_post_carriage()
{
	define_param("WD", m_proto.get_total_width());
}

