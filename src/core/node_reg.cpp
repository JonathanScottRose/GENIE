#include "pch.h"
#include "node_reg.h"
#include "net_rs.h"
#include "port_clockreset.h"
#include "port_rs.h"
#include "genie_priv.h"

using namespace genie::impl;
using hdl::PortBindingRef;

namespace
{
	const char MODNAME[] = "genie_pipe_stage";

	const char INPORT_NAME[] = "in";
	const char OUTPORT_NAME[] = "out";
	const char CLOCKPORT_NAME[] = "clock";
	const char RESETPORT_NAME[] = "reset";

	PrimDB* s_prim_db;
	SMART_ENUM(DB_COLS, WIDTH, BP);
	SMART_ENUM(DB_SRC, I_VALID, I_READY, I_DATA, INT);
	SMART_ENUM(DB_SINK, O_VALID, O_READY, O_DATA, INT);
}

void NodeReg::init()
{
	genie::impl::register_reserved_module(MODNAME);
	s_prim_db = genie::impl::load_prim_db(MODNAME,
		DB_COLS::get_table(), DB_SRC::get_table(), DB_SINK::get_table());
}

NodeReg::NodeReg()
	: Node(MODNAME, MODNAME)
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

	auto link = (LinkRSPhys*)connect(inport, outport, NET_RS_PHYS);
	link->set_latency(1);
}

NodeReg::NodeReg(const NodeReg& o)
	: Node(o), ProtocolCarrier(o)
{
	// Create a copy of an existing NodeReg
}

void NodeReg::init_vlog()
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

NodeReg* NodeReg::clone() const
{
	return new NodeReg(*this);
}

PortRS * NodeReg::get_input() const
{
	return get_port<PortRS>(INPORT_NAME);
}

PortRS * NodeReg::get_output() const
{
	return get_port<PortRS>(OUTPORT_NAME);
}

PortClock * NodeReg::get_clock_port() const
{
	return static_cast<PortClock*>(get_port(CLOCKPORT_NAME));
}

void NodeReg::prepare_for_hdl()
{
	auto& proto = get_carried_proto();
	set_int_param("WIDTH", proto.get_total_width());
}

void NodeReg::annotate_timing()
{
	// pointless for now - these are added AFTER constraint satisfaction
}

AreaMetrics NodeReg::annotate_area()
{
	AreaMetrics result;
	unsigned node_width = get_carried_proto().get_total_width();
	bool bp = get_output()->get_bp_status().status == RSBackpressure::ENABLED;
	unsigned col_vals[DB_COLS::size()];

	col_vals[DB_COLS::BP] = bp ? 1 : 0;

	auto& ap = genie::impl::get_arch_params();

	if (node_width == 0)
	{
		col_vals[DB_COLS::WIDTH] = 0;
		auto row = s_prim_db->get_row(col_vals);
		assert(row);
		auto metrics = s_prim_db->get_area_metrics(row);
		assert(metrics);
		result = *metrics;
	}
	else
	{
		// width 1
		col_vals[DB_COLS::WIDTH] = 1;
		auto row = s_prim_db->get_row(col_vals);
		assert(row);
		auto metrics_1 = s_prim_db->get_area_metrics(row);
		assert(metrics_1);

		// width 2
		col_vals[DB_COLS::WIDTH] = 2;
		row = s_prim_db->get_row(col_vals);
		assert(row);
		auto metrics_2 = s_prim_db->get_area_metrics(row);
		assert(metrics_2);

		result = *metrics_1 + (*metrics_2 - *metrics_1)*node_width;
	}

	return result;
}


