#include "pch.h"
#include "node_split.h"
#include "net_topo.h"
#include "port_rs.h"
#include "port_clockreset.h"
#include "genie_priv.h"

using namespace genie::impl;

namespace
{
    const char MODNAME[] = "genie_split";

	const char INPORT_NAME[] = "in";
	const char OUTPORT_NAME[] = "out";
	const char CLOCKPORT_NAME[] = "clock";
	const char RESETPORT_NAME[] = "reset";
}

void NodeSplit::init()
{
	genie::impl::register_reserved_module(MODNAME);
}

NodeSplit::NodeSplit()
    : Node(MODNAME, MODNAME)
{
	init_vlog();

	// Clock and reset ports
	add_port(new PortClock(CLOCKPORT_NAME, Port::Dir::IN, "clk"));
	add_port(new PortReset(RESETPORT_NAME, Port::Dir::IN, "reset"));

	auto inport = new PortRS(INPORT_NAME, Port::Dir::IN, CLOCKPORT_NAME);
	inport->add_subport(PortRS::Role::DATA_CARRIER, { "i_data", "WO" });
	inport->add_subport(PortRS::Role::DATABUNDLE, "flow_id", { "i_flow", "WF" });
	inport->add_subport(PortRS::Role::VALID, "i_valid");
	inport->add_subport(PortRS::Role::READY, "o_ready");
	add_port(inport);

	// The node itself can be connected to with TOPO links
	make_connectable(NET_TOPO);
}

NodeSplit::NodeSplit(const NodeSplit& o)
    : Node(o)
{	
    // Create a copy of an existing NodeSplit
}

void NodeSplit::init_vlog()
{
	using Dir = hdl::Port::Dir;

	auto& hdl = get_hdl_state();
	hdl.add_port("clk", 1, 1, Dir::IN);
	hdl.add_port("reset", 1, 1, Dir::IN);
	hdl.add_port("i_data", "WO", 1, Dir::IN);
	hdl.add_port("i_flow", "WF", 1, Dir::IN);
	hdl.add_port("i_valid", 1, 1, Dir::IN);
	hdl.add_port("o_ready", 1, 1, Dir::OUT);
	hdl.add_port("o_valid", "NO", 1, Dir::OUT);
	hdl.add_port("o_data", "WO", 1, Dir::OUT);
	hdl.add_port("o_flow", "WF", 1, Dir::OUT);
	hdl.add_port("i_ready", "NO", 1, Dir::IN);
}

Node* NodeSplit::instantiate()
{
    return new NodeSplit(*this);
}