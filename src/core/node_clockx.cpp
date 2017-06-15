#include "pch.h"
#include "node_clockx.h"
#include "port_clockreset.h"
#include "port_rs.h"
#include "net_rs.h"
#include "prim_db.h"

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

	PrimDB* s_prim_db;
	SMART_ENUM(DB_COLS, WIDTH, BP);
	SMART_ENUM(DB_SRC, I_DATA, I_VALID, I_READY, INT);
	SMART_ENUM(DB_SINK, O_DATA, O_VALID, O_READY, INT);
}

void NodeClockX::init()
{
	genie::impl::register_reserved_module(MODNAME);

	s_prim_db = genie::impl::load_prim_db(MODNAME,
		DB_COLS::get_table(), DB_SRC::get_table(), DB_SINK::get_table());
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

	auto int_link = static_cast<LinkRSPhys*>(connect(inport, outport, NET_RS_PHYS));
	// TODO: this is a nonsensical value, since the input and output are on different
	// clock domains. However, it allows other parts of GENIE to at least treat
	// the internal link as a non-purely-combinational path.
	int_link->set_latency(2);
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

void NodeClockX::annotate_timing()
{
	bool bp = get_outdata_port()->get_bp_status().status == RSBackpressure::ENABLED;
	unsigned col_vals[DB_COLS::size()];

	col_vals[DB_COLS::BP] = bp ? 1 : 0;
	col_vals[DB_COLS::WIDTH] = 1; // irrelevant for timing
	auto row = s_prim_db->get_row(col_vals);
	auto tnodes = s_prim_db->get_tnodes(row);
	assert(row);
	assert(tnodes);

	unsigned in_delay = 0;
	unsigned out_delay = 0;

	in_delay = std::max(
		s_prim_db->get_tnode_val(tnodes, DB_SRC::I_DATA, DB_SINK::INT),
		s_prim_db->get_tnode_val(tnodes, DB_SRC::I_VALID, DB_SINK::INT)
	);
	
	out_delay = std::max(
		s_prim_db->get_tnode_val(tnodes, DB_SRC::INT, DB_SINK::O_DATA),
		s_prim_db->get_tnode_val(tnodes, DB_SRC::INT, DB_SINK::O_VALID)
	);

	if (bp)
	{
		in_delay =
			std::max(in_delay, s_prim_db->get_tnode_val(tnodes, DB_SRC::I_READY, DB_SINK::INT));

		out_delay =
			std::max(out_delay, s_prim_db->get_tnode_val(tnodes, DB_SRC::INT, DB_SINK::O_READY));
	}

	get_indata_port()->set_logic_depth(in_delay);
	get_outdata_port()->set_logic_depth(out_delay);
}

AreaMetrics NodeClockX::annotate_area()
{
	AreaMetrics result;

	unsigned node_width = get_carried_proto().get_total_width();
	bool bp = get_outdata_port()->get_bp_status().status == RSBackpressure::ENABLED;
	unsigned col_vals[DB_COLS::size()];
	
	col_vals[DB_COLS::BP] = bp ? 1 : 0;

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
		col_vals[DB_COLS::WIDTH] = 2;
		auto row = s_prim_db->get_row(col_vals);
		assert(row);
		auto metrics_2 = s_prim_db->get_area_metrics(row);
		assert(metrics_2);
		
		col_vals[DB_COLS::WIDTH] = 1;
		row = s_prim_db->get_row(col_vals);
		assert(row);
		auto metrics_1 = s_prim_db->get_area_metrics(row);
		assert(metrics_1);

		result = *metrics_1 + (*metrics_2 - *metrics_1)*node_width;
	}

	return result;
}

