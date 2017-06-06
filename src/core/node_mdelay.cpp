#include "pch.h"
#include "node_mdelay.h"
#include "net_rs.h"
#include "port_clockreset.h"
#include "port_rs.h"
#include "genie_priv.h"

using namespace genie::impl;
using hdl::PortBindingRef;

namespace
{
	const char MODNAME[] = "genie_mem_delay";

	const char INPORT_NAME[] = "in";
	const char OUTPORT_NAME[] = "out";
	const char CLOCKPORT_NAME[] = "clock";
	const char RESETPORT_NAME[] = "reset";
}

void NodeMDelay::init()
{
	genie::impl::register_reserved_module(MODNAME);
}

NodeMDelay::NodeMDelay()
	: Node(MODNAME, MODNAME), m_delay(0)
{
	init_vlog();

	// Clock and reset ports
	add_port(new PortClock(CLOCKPORT_NAME, Port::Dir::IN, "clk"));
	add_port(new PortReset(RESETPORT_NAME, Port::Dir::IN, "reset"));

	auto inport = new PortRS(INPORT_NAME, Port::Dir::IN, CLOCKPORT_NAME);
	inport->add_role_binding(PortRS::VALID, "i_valid");
	inport->add_role_binding(PortRS::READY, "o_ready");
	inport->add_role_binding(PortRS::DATA_CARRIER, { "i_data", "WIDTH" });
	inport->get_bp_status().make_configurable();
	add_port(inport);

	auto outport = new PortRS(OUTPORT_NAME, Port::Dir::OUT, CLOCKPORT_NAME);
	outport->add_role_binding(PortRS::VALID, "o_valid");
	outport->add_role_binding(PortRS::READY, "i_ready");
	outport->add_role_binding(PortRS::DATA_CARRIER, { "o_data", "WIDTH" });
	outport->get_bp_status().make_configurable();
	add_port(outport);

	connect(inport, outport, NET_RS_PHYS);
}

NodeMDelay::NodeMDelay(const NodeMDelay& o)
	: Node(o), ProtocolCarrier(o), m_delay(o.m_delay)
{
	// Create a copy of an existing NodeMDelay
}

void NodeMDelay::init_vlog()
{
	using Dir = hdl::Port::Dir;

	auto& hdl = get_hdl_state();
	hdl.add_port("clk", 1, 1, Dir::IN);
	hdl.add_port("reset", 1, 1, Dir::IN);
	hdl.add_port("i_valid", 1, 1, Dir::IN);
	hdl.add_port("o_valid", 1, 1, Dir::OUT);
	hdl.add_port("i_ready", 1, 1, Dir::IN);
	hdl.add_port("o_ready", 1, 1, Dir::OUT);
	hdl.add_port("i_data", "WIDTH", 1, Dir::IN);
	hdl.add_port("o_data", "WIDTH", 1, Dir::OUT);
}

NodeMDelay* NodeMDelay::clone() const
{
	return new NodeMDelay(*this);
}

PortRS * NodeMDelay::get_input() const
{
	return get_port<PortRS>(INPORT_NAME);
}

PortRS * NodeMDelay::get_output() const
{
	return get_port<PortRS>(OUTPORT_NAME);
}

PortClock * NodeMDelay::get_clock_port() const
{
	return static_cast<PortClock*>(get_port(CLOCKPORT_NAME));
}

void NodeMDelay::set_delay(unsigned delay)
{
	// We should not be using this for just 1 cycle
	assert(delay > 1);

	m_delay = delay;

	// Adjust internal link
	auto link = (LinkRSPhys*)get_links(get_input(), get_output(), NET_RS_PHYS).front();
	link->set_latency(delay);
}

void NodeMDelay::prepare_for_hdl()
{
	auto& proto = get_carried_proto();
	set_int_param("WIDTH", proto.get_total_width());

	set_int_param("CYCLES", m_delay);
}


