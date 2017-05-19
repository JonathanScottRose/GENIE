#include "pch.h"
#include "node_clockx.h"
#include "port_clockreset.h"
#include "port_rs.h"
#include "net_rs.h"

using namespace genie::impl;
using hdl::PortBindingRef;

namespace
{
	const char MODNAME[] = "genie_clockx";

	const char INDATAPORT_NAME[] = "in_data";
	const char INCLOCKPORT_NAME[] = "in_clock";
	const char OUTDATAPORT_NAME[] = "out_data";
	const char OUTCLOCKPORT_NAME[] = "out_clock";
	const char RESETPORT_NAME[] = "reset";
}

void NodeClockX::init()
{
	genie::impl::register_reserved_module(MODNAME);
}

void NodeClockX::init_vlog()
{
	using Dir = hdl::Port::Dir;

	auto& hdl = get_hdl_state();
	hdl.add_port("arst", 1, 1, Dir::IN);
	hdl.add_port("wrclk", 1, 1, Dir::IN);
	hdl.add_port("rdclk", 1, 1, Dir::IN);
	hdl.add_port("i_data", "WIDTH", 1, Dir::IN);
	hdl.add_port("i_valid", 1, 1, Dir::IN);
	hdl.add_port("o_ready", 1, 1, Dir::OUT);
	hdl.add_port("o_data", "WIDTH", 1, Dir::OUT);
	hdl.add_port("o_valid", 1, 1, Dir::OUT);
	hdl.add_port("i_ready", 1, 1, Dir::IN);
}


NodeClockX::NodeClockX()
	: Node(MODNAME, MODNAME)
{
	init_vlog();

	add_port(new PortClock(INCLOCKPORT_NAME, Port::Dir::IN, "wrclk"));
	add_port(new PortClock(OUTCLOCKPORT_NAME, Port::Dir::IN, "rdclk"));
	add_port(new PortReset(RESETPORT_NAME, Port::Dir::IN, "arst"));

	auto inport = new PortRS(INDATAPORT_NAME, Port::Dir::IN, INCLOCKPORT_NAME);
	inport->add_role_binding(PortRS::VALID, "i_valid");		
	inport->add_role_binding(PortRS::DATA_CARRIER, "i_data");
    inport->add_role_binding(PortRS::READY, "o_ready");
    inport->get_bp_status().force_enable();
	add_port(inport);

	auto outport = new PortRS(OUTDATAPORT_NAME, Port::Dir::OUT, OUTCLOCKPORT_NAME);
	outport->add_role_binding(PortRS::VALID, "o_valid");
	outport->add_role_binding(PortRS::DATA_CARRIER, "o_data");
    outport->add_role_binding(PortRS::READY, "i_ready");
    outport->get_bp_status().make_configurable();
	add_port(outport);

	//connect(inport, outport, NET_RS_PHYS); // don't
}

PortClock* NodeClockX::get_inclock_port() const
{
	return (PortClock*)get_port(INCLOCKPORT_NAME);
}

PortClock* NodeClockX::get_outclock_port() const
{
	return (PortClock*)get_port(OUTCLOCKPORT_NAME);
}

PortRS* NodeClockX::get_indata_port() const
{
	return (PortRS*)get_port(INDATAPORT_NAME);
}

PortRS* NodeClockX::get_outdata_port() const
{
	return (PortRS*)get_port(OUTDATAPORT_NAME);
}

NodeClockX* NodeClockX::clone() const
{
	return new NodeClockX(*this);
}

void NodeClockX::prepare_for_hdl()
{
	auto& proto = get_carried_proto();
	set_int_param("WIDTH", proto.get_total_width());
}

