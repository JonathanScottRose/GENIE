#include "pch.h"
#include "node_mdelay.h"
#include "net_rs.h"
#include "port_clockreset.h"
#include "port_rs.h"
#include "genie_priv.h"
#include "prim_db.h"

using namespace genie::impl;
using hdl::PortBindingRef;

namespace
{
	const char MODNAME[] = "genie_mem_delay";

	const char INPORT_NAME[] = "in";
	const char OUTPORT_NAME[] = "out";
	const char CLOCKPORT_NAME[] = "clock";
	const char RESETPORT_NAME[] = "reset";

	PrimDB* s_prim_db;
	SMART_ENUM(DB_COLS, WIDTH, CYCLES, BP);
	SMART_ENUM(DB_SRC, I_VALID, I_READY, I_DATA, INT);
	SMART_ENUM(DB_SINK, O_VALID, O_READY, O_DATA, INT);
}

void NodeMDelay::init()
{
	genie::impl::register_reserved_module(MODNAME);

	s_prim_db = genie::impl::load_prim_db(MODNAME,
		DB_COLS::get_table(), DB_SRC::get_table(), DB_SINK::get_table());
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

void NodeMDelay::annotate_timing()
{
	// kinda pointless, since this is inserted after reg constraints
}

AreaMetrics NodeMDelay::annotate_area()
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
		col_vals[DB_COLS::CYCLES] = 2;
		auto row = s_prim_db->get_row(col_vals);
		assert(row);
		auto metrics = s_prim_db->get_area_metrics(row);
		assert(metrics);
		result = *metrics;
	}
	else
	{
		// Base cycles, base width
		col_vals[DB_COLS::WIDTH] = 1;
		col_vals[DB_COLS::CYCLES] = 2;
		auto row = s_prim_db->get_row(col_vals);
		assert(row);
		auto metrics_base = s_prim_db->get_area_metrics(row);
		assert(metrics_base);

		// Base cycles, next width
		col_vals[DB_COLS::WIDTH] = (ap.lutram_width + 1);
		col_vals[DB_COLS::CYCLES] = 2;
		row = s_prim_db->get_row(col_vals);
		assert(row);
		auto metrics_width = s_prim_db->get_area_metrics(row);
		assert(metrics_width);

		// Next cycles, base width
		col_vals[DB_COLS::WIDTH] = 1;
		col_vals[DB_COLS::CYCLES] = (ap.lutram_depth + 1);
		row = s_prim_db->get_row(col_vals);
		assert(row);
		auto metrics_depth = s_prim_db->get_area_metrics(row);
		assert(metrics_depth);

		unsigned w_slices = (node_width + ap.lutram_width - 1) / ap.lutram_width;
		unsigned d_slices = (m_delay + ap.lutram_depth - 1) / ap.lutram_depth;

		result = *metrics_base +
			(*metrics_width - *metrics_base)*w_slices +
			(*metrics_depth - *metrics_base)*d_slices;
	}

	return result;
}
