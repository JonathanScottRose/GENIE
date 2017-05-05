#include "pch.h"
#include "node_conv.h"
#include "net_rs.h"
#include "port_clockreset.h"
#include "port_rs.h"
#include "genie_priv.h"

using namespace genie::impl;
using hdl::PortBindingRef;

namespace
{
	const char MODNAME[] = "genie_conv";

	const char INPORT_NAME[] = "in";
	const char OUTPORT_NAME[] = "out";
	const char CLOCKPORT_NAME[] = "clock";
	const char RESETPORT_NAME[] = "reset";
}

void NodeConv::init()
{
	genie::impl::register_reserved_module(MODNAME);
}

NodeConv::NodeConv()
	: Node(MODNAME, MODNAME), m_in_width(0), m_out_width(0)
{
	init_vlog();

	// Clock and reset ports
	add_port(new PortClock(CLOCKPORT_NAME, Port::Dir::IN, "clk"));
	add_port(new PortReset(RESETPORT_NAME, Port::Dir::IN, "reset"));

	auto inport = new PortRS(INPORT_NAME, Port::Dir::IN, CLOCKPORT_NAME);
	inport->add_subport(PortRS::VALID, "i_valid");
	inport->add_subport(PortRS::DATA, { "i_in", "WIDTH_IN" });
	add_port(inport);

	auto outport = new PortRS(OUTPORT_NAME, Port::Dir::OUT, CLOCKPORT_NAME);
	outport->add_subport(PortRS::DATA, { "o_out", "WIDTH_OUT" });
	add_port(outport);

	connect(inport, outport, NET_RS);
}

NodeConv::NodeConv(const NodeConv& o)
	: Node(o), m_in_width(o.m_in_width), m_out_width(o.m_out_width),
	m_table(o.m_table)
{
	// Create a copy of an existing NodeConv
}

void NodeConv::init_vlog()
{
	using Dir = hdl::Port::Dir;

	auto& hdl = get_hdl_state();
	hdl.add_port("clk", 1, 1, Dir::IN);
	hdl.add_port("reset", 1, 1, Dir::IN);
	hdl.add_port("i_valid", 1, 1, Dir::IN);
	hdl.add_port("i_in", "WIDTH_IN", 1, Dir::IN);
	hdl.add_port("o_out", "WIDTH_OUT", 1, Dir::OUT);
}

NodeConv* NodeConv::clone() const
{
	return new NodeConv(*this);
}


