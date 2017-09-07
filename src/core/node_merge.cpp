#include "pch.h"
#include "node_merge.h"
#include "net_topo.h"
#include "net_rs.h"
#include "port_clockreset.h"
#include "port_rs.h"
#include "genie_priv.h"
#include "prim_db.h"

using namespace genie::impl;
using hdl::PortBindingRef;

namespace
{
    const char MODNAME[] = "genie_merge";
	const char MODNAME_EX[] = "genie_merge_ex";

	const char INPORT_NAME[] = "in";
	const char OUTPORT_NAME[] = "out";
	const char CLOCKPORT_NAME[] = "clock";
	const char RESETPORT_NAME[] = "reset";

	PrimDB* s_prim_db;
	SMART_ENUM(DB_COLS, NI, WIDTH, BP, EOP);
	SMART_ENUM(DB_SRC, I_VALID, I_READY, I_DATA, I_EOP, INT);
	SMART_ENUM(DB_SINK, O_VALID, O_READY, O_DATA, O_EOP, INT);

	PrimDB* s_prim_db_ex;
	SMART_ENUM(DB_EX_COLS, NI, WIDTH);
	SMART_ENUM(DB_EX_SRC, I_VALID, I_DATA, I_EOP);
	SMART_ENUM(DB_EX_SINK, O_VALID, O_DATA, O_EOP);
}

void NodeMerge::init()
{
	genie::impl::register_reserved_module(MODNAME);
	genie::impl::register_reserved_module(MODNAME_EX);

	s_prim_db = genie::impl::load_prim_db(MODNAME,
		DB_COLS::get_table(), DB_SRC::get_table(), DB_SINK::get_table());
	s_prim_db_ex = genie::impl::load_prim_db(MODNAME_EX,
		DB_EX_COLS::get_table(), DB_EX_SRC::get_table(), DB_EX_SINK::get_table());
}

NodeMerge::NodeMerge()
    : Node(MODNAME, MODNAME), m_n_inputs(0), m_is_exclusive(false)
{
	init_vlog();

	// Clock and reset ports
	add_port(new PortClock(CLOCKPORT_NAME, Port::Dir::IN, "clk"));
	add_port(new PortReset(RESETPORT_NAME, Port::Dir::IN, "reset"));
	
	auto outport = new PortRS(OUTPORT_NAME, Port::Dir::OUT, CLOCKPORT_NAME);
	outport->add_role_binding(PortRS::DATA_CARRIER, { "o_data", "WIDTH" });
	outport->add_role_binding(PortRS::VALID, "o_valid");
	outport->add_role_binding(PortRS::READY, "i_ready");
	outport->add_role_binding(PortRS::EOP, "o_eop");
	outport->get_bp_status().make_configurable();
	add_port(outport);

	auto& proto = outport->get_proto();
	proto.add_terminal_field({ FIELD_EOP, 1 }, PortRS::EOP);

	// The node itself can be connected to with TOPO links
	make_connectable(NET_TOPO);
	get_endpoint(NET_TOPO, Port::Dir::IN)->set_max_links(Endpoint::UNLIMITED);
}

void NodeMerge::init_vlog()
{
	using Dir = hdl::Port::Dir;

	auto& hdl = get_hdl_state();
	hdl.add_port("clk", 1, 1, Dir::IN);
	hdl.add_port("reset", 1, 1, Dir::IN);
	hdl.add_port("i_data", "WIDTH", "NI", Dir::IN);
	hdl.add_port("i_valid", "NI", 1, Dir::IN);
	hdl.add_port("i_eop", "NI", 1, Dir::IN);
	hdl.add_port("o_ready", "NI", 1, Dir::OUT);
	hdl.add_port("o_valid", 1, 1, Dir::OUT);
	hdl.add_port("o_eop", 1, 1, Dir::OUT);
	hdl.add_port("o_data", "WIDTH", 1, Dir::OUT);
	hdl.add_port("i_ready", 1, 1, Dir::IN);
}

NodeMerge* NodeMerge::clone() const
{
    return new NodeMerge(*this);
}

void NodeMerge::create_ports()
{
	// Get incoming topo links
	auto& tlinks = get_endpoint(NET_TOPO, Port::Dir::IN)->links();
	m_n_inputs = tlinks.size();

	auto outp = get_output();
	outp->get_endpoint(NET_RS_PHYS, Port::Dir::IN)->set_max_links(m_n_inputs);

	// Create child ports
	for (unsigned i = 0; i < m_n_inputs; i++)
	{
		auto p = new PortRS(INPORT_NAME + std::to_string(i), Port::Dir::IN, CLOCKPORT_NAME);
		p->add_role_binding(PortRS::VALID, PortBindingRef("i_valid").set_lo_bit(i));
		p->add_role_binding(PortRS::DATA_CARRIER,
			PortBindingRef("i_data", "WIDTH").set_lo_slice(i));
		p->add_role_binding(PortRS::EOP, PortBindingRef("i_eop").set_lo_bit(i));
		p->add_role_binding(PortRS::READY, PortBindingRef("o_ready").set_lo_bit(i));
		
		// A non-exclusive merge node generates, rather than simply carries, backpressure.
		if (m_is_exclusive)
			p->get_bp_status().make_configurable();
		else
			p->get_bp_status().force_enable();
		
		add_port(p);

		auto& proto = p->get_proto();
		proto.add_terminal_field({ FIELD_EOP, 1 }, PortRS::EOP);

		// Connect to output port, for traversal
		connect(p, outp, NET_RS_PHYS);
	}
}

PortRS * NodeMerge::get_input(unsigned i) const
{
	return get_child_as<PortRS>(INPORT_NAME + std::to_string(i));
}

PortRS * NodeMerge::get_output() const
{
	return get_child_as<PortRS>(OUTPORT_NAME);
}

void NodeMerge::prepare_for_hdl()
{
	auto& proto = get_carried_proto();
	set_int_param("WIDTH", proto.get_total_width());
	set_int_param("NI", m_n_inputs);

	// This assumes it was set to MODNAME by default before
	if (m_is_exclusive)
		set_hdl_name(MODNAME_EX);
}

void NodeMerge::annotate_timing()
{
	m_is_exclusive ? annotate_timing_ex() : annotate_timing_nonex();
}

AreaMetrics NodeMerge::annotate_area()
{
	return m_is_exclusive ? annotate_area_ex() : annotate_area_nonex();
}

void NodeMerge::annotate_timing_nonex()
{
	// Get merge node config
	unsigned width = get_carried_proto().get_total_width();
	bool bp = get_output()->get_bp_status().status == RSBackpressure::ENABLED;
	bool eop = false;
	for (unsigned i = 0; i < m_n_inputs; i++)
	{
		// If any input has a non-const EOP, then EOP is used
		if (get_input(i)->get_proto().get_const(FIELD_EOP) == nullptr)
		{
			eop = true;
			break;
		}
	}
	
	unsigned col_vals[DB_COLS::size()];
	col_vals[DB_COLS::NI] = m_n_inputs; assert(m_n_inputs <= 32);
	col_vals[DB_COLS::BP] = bp ? 1 : 0;
	col_vals[DB_COLS::EOP] = eop ? 1 : 0;
	col_vals[DB_COLS::WIDTH] = 1;

	auto row = s_prim_db->get_row(col_vals);
	auto tnodes = s_prim_db->get_tnodes(row);
	assert(row);
	assert(tnodes);

	unsigned in_delay = 0;
	unsigned out_delay = 0;
	unsigned thru_delay = 0;

	for (auto src : { DB_SRC::I_VALID, DB_SRC::I_EOP, DB_SRC::I_READY })
	{
		in_delay = std::max(in_delay,
			s_prim_db->get_tnode_val(tnodes, src, DB_SINK::INT));
	}

	for (auto sink : { DB_SINK::O_EOP, DB_SINK::O_VALID, DB_SINK::O_DATA, DB_SINK::O_READY })
	{
		in_delay = std::max(in_delay,
			s_prim_db->get_tnode_val(tnodes, DB_SRC::INT, sink));
	}

	for (auto pair : {
		std::make_pair(DB_SRC::I_DATA, DB_SINK::O_DATA),
		std::make_pair(DB_SRC::I_VALID, DB_SINK::O_VALID),
		std::make_pair(DB_SRC::I_VALID, DB_SINK::O_DATA),
		std::make_pair(DB_SRC::I_VALID, DB_SINK::O_EOP),
		std::make_pair(DB_SRC::I_VALID, DB_SINK::O_READY),
		std::make_pair(DB_SRC::I_EOP, DB_SINK::O_EOP),
		std::make_pair(DB_SRC::I_READY, DB_SINK::O_READY) })
	{
		thru_delay = std::max(thru_delay,
			s_prim_db->get_tnode_val(tnodes, pair.first, pair.second));
	}

	for (unsigned i = 0; i < m_n_inputs; i++)
	{
		auto p = get_input(i);
		p->set_logic_depth(in_delay);
		auto lnk = (LinkRSPhys*)p->get_endpoint(NET_RS_PHYS, Port::Dir::OUT)->get_link0();
		lnk->set_logic_depth(thru_delay);
	}

	get_output()->set_logic_depth(out_delay);
}

void NodeMerge::annotate_timing_ex()
{
	// Get merge node config
	unsigned width = get_carried_proto().get_total_width();

	unsigned col_vals[DB_EX_COLS::size()];
	col_vals[DB_EX_COLS::NI] = m_n_inputs; assert(m_n_inputs <= 32);
	col_vals[DB_EX_COLS::WIDTH] = 1;

	auto row = s_prim_db_ex->get_row(col_vals);
	auto tnodes = s_prim_db_ex->get_tnodes(row);
	assert(row);
	assert(tnodes);

	unsigned thru_delay = 0;

	for (auto pair : {
		std::make_pair(DB_EX_SRC::I_DATA, DB_EX_SINK::O_DATA),
		std::make_pair(DB_EX_SRC::I_VALID, DB_EX_SINK::O_VALID),
		std::make_pair(DB_EX_SRC::I_VALID, DB_EX_SINK::O_DATA),
		std::make_pair(DB_EX_SRC::I_VALID, DB_EX_SINK::O_EOP),
		std::make_pair(DB_EX_SRC::I_EOP, DB_EX_SINK::O_EOP) })
	{
		thru_delay = std::max(thru_delay,
			s_prim_db_ex->get_tnode_val(tnodes, pair.first, pair.second));
	}

	for (auto lnk : get_output()->get_endpoint(NET_RS_PHYS, Port::Dir::IN)->links())
	{
		((LinkRSPhys*)lnk)->set_logic_depth(thru_delay);
	}
}

AreaMetrics NodeMerge::annotate_area_nonex()
{
	AreaMetrics result;
	unsigned node_width = get_carried_proto().get_total_width();
	bool bp = get_output()->get_bp_status().status == RSBackpressure::ENABLED;
	bool eop = false;
	for (unsigned i = 0; i < m_n_inputs; i++)
	{
		// If any input has a non-const EOP, then EOP is used
		if (get_input(i)->get_proto().get_const(FIELD_EOP) == nullptr)
		{
			eop = true;
			break;
		}
	}
	unsigned col_vals[DB_COLS::size()];

	col_vals[DB_COLS::BP] = bp ? 1 : 0;
	col_vals[DB_COLS::EOP] = eop ? 1 : 0;
	col_vals[DB_COLS::NI] = m_n_inputs;

	// SPECIAL CASE for altera devices:
	// a 2-to-1 mux driving a register will use zero area for the mux
	bool radix2_driving_reg =
		m_n_inputs == 2 && node_width >= 7 && does_feed_reg();

	if (node_width == 0)
	{
		col_vals[DB_COLS::WIDTH] = 0;
		auto row = s_prim_db->get_row(col_vals);
		assert(row);
		auto metrics = s_prim_db->get_area_metrics(row);
		assert(metrics);
		result = *metrics;
	}
	else if (radix2_driving_reg)
	{
		// for now: approximate with width=1 (a very small 2-to-1 mux)
		col_vals[DB_COLS::WIDTH] = 1;
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

AreaMetrics NodeMerge::annotate_area_ex()
{
	AreaMetrics result;
	unsigned node_width = get_carried_proto().get_total_width();

	unsigned col_vals[DB_EX_COLS::size()];
	col_vals[DB_EX_COLS::NI] = m_n_inputs;

	bool radix2_driving_reg =
		m_n_inputs == 2 && node_width >= 7 && does_feed_reg();

	if (node_width == 0)
	{
		col_vals[DB_EX_COLS::WIDTH] = 0;
		auto row = s_prim_db_ex->get_row(col_vals);
		assert(row);
		auto metrics = s_prim_db_ex->get_area_metrics(row);
		assert(metrics);
		result = *metrics;
	}
	else if (radix2_driving_reg)
	{
		// for now: approximate with width=1 (a very small 2-to-1 mux)
		col_vals[DB_EX_COLS::WIDTH] = 1;
		auto row = s_prim_db_ex->get_row(col_vals);
		assert(row);
		auto metrics = s_prim_db_ex->get_area_metrics(row);
		assert(metrics);
		result = *metrics;
	}
	else
	{
		// width 1
		col_vals[DB_EX_COLS::WIDTH] = 1;
		auto row = s_prim_db_ex->get_row(col_vals);
		assert(row);
		auto metrics_1 = s_prim_db_ex->get_area_metrics(row);
		assert(metrics_1);

		// width 2
		col_vals[DB_EX_COLS::WIDTH] = 2;
		row = s_prim_db_ex->get_row(col_vals);
		assert(row);
		auto metrics_2 = s_prim_db_ex->get_area_metrics(row);
		assert(metrics_2);

		result = *metrics_1 + (*metrics_2 - *metrics_1)*node_width;
	}

	return result;
}

bool genie::impl::NodeMerge::does_feed_reg()
{
	// Get physical output of merge node
	auto phys_out = get_output()->get_endpoint(NET_RS_PHYS, Port::Dir::OUT);

	// Get remote PortRS
	auto remote_rs = dynamic_cast<PortRS*>(phys_out->get_remote_obj0());
	if (!remote_rs)
		return false;

	// Zero logic depth === directly feeds a register
	return remote_rs->get_logic_depth() == 0;
}
