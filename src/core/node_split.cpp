#include "pch.h"
#include "node_split.h"
#include "net_topo.h"
#include "net_rs.h"
#include "port_rs.h"
#include "port_clockreset.h"
#include "address.h"
#include "genie_priv.h"

using namespace genie::impl;
using hdl::PortBindingRef;

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
    : Node(MODNAME, MODNAME), m_n_outputs(0)
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
	get_endpoint(NET_TOPO, Port::Dir::OUT)->set_max_links(Endpoint::UNLIMITED);
}

NodeSplit::~NodeSplit()
{
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
	hdl.add_port("i_ready", "NO", 1, Dir::IN);
}

NodeSplit* NodeSplit::clone() const
{
    return new NodeSplit(*this);
}

void NodeSplit::create_ports()
{
	// Get incoming topo links
	auto& tlinks = get_endpoint(NET_TOPO, Port::Dir::OUT)->links();
	m_n_outputs = tlinks.size();

	auto inp = get_input();

	// Create child ports
	for (unsigned i = 0; i < m_n_outputs; i++)
	{
		auto p = new PortRS(OUTPORT_NAME + std::to_string(i), Port::Dir::OUT, CLOCKPORT_NAME);
		p->add_subport(PortRS::Role::VALID, PortBindingRef("o_valid").set_lo_bit(i));
		p->add_subport(PortRS::Role::DATA_CARRIER, PortBindingRef("o_data", "WO"));
		p->add_subport(PortRS::Role::READY, PortBindingRef("i_ready").set_lo_bit(i));
		add_port(p);

		// Connect in->out ports for traversal
		connect(inp, p, NET_RS);
	}
}

PortRS * NodeSplit::get_output(unsigned i) const
{
	return get_child_as<PortRS>(OUTPORT_NAME + std::to_string(i));
}

PortRS * NodeSplit::get_input() const
{
	return get_child_as<PortRS>(INPORT_NAME);
}