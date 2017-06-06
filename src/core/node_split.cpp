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

	PrimDB* s_prim_db;
	SMART_ENUM(DB_COLS, NO_MULTICAST, N, BP);
	SMART_ENUM(DB_SRC, I_VALID, I_MASK, I_READY, INT);
	SMART_ENUM(DB_SINK, INT, O_VALID, O_READY);
}

FieldType genie::impl::FIELD_SPLITMASK;

void NodeSplit::init()
{
	genie::impl::register_reserved_module(MODNAME);
	s_prim_db = genie::impl::load_prim_db(MODNAME,
		DB_COLS::get_table(), DB_SRC::get_table(), DB_SINK::get_table());
	FIELD_SPLITMASK = register_field();
}

NodeSplit::NodeSplit()
    : Node(MODNAME, MODNAME), m_n_outputs(0)
{
	init_vlog();

	// Clock and reset ports
	add_port(new PortClock(CLOCKPORT_NAME, Port::Dir::IN, "clk"));
	add_port(new PortReset(RESETPORT_NAME, Port::Dir::IN, "reset"));

	auto inport = new PortRS(INPORT_NAME, Port::Dir::IN, CLOCKPORT_NAME);
	inport->add_role_binding(PortRS::DATA_CARRIER, { "i_data", "WIDTH" });
	inport->add_role_binding(PortRS::ADDRESS, { "i_mask", "N" });
	inport->add_role_binding(PortRS::VALID, "i_valid");
	inport->add_role_binding(PortRS::READY, "o_ready");
	inport->get_bp_status().make_configurable();
	add_port(inport);

	// The node itself can be connected to with TOPO links
	make_connectable(NET_TOPO);
	get_endpoint(NET_TOPO, Port::Dir::OUT)->set_max_links(Endpoint::UNLIMITED);
}

NodeSplit::~NodeSplit()
{
}

NodeSplit::NodeSplit(const NodeSplit& o)
    : Node(o), ProtocolCarrier(o), m_n_outputs(o.m_n_outputs)
{	
    // Create a copy of an existing NodeSplit
}

void NodeSplit::init_vlog()
{
	using Dir = hdl::Port::Dir;

	auto& hdl = get_hdl_state();
	hdl.add_port("clk", 1, 1, Dir::IN);
	hdl.add_port("reset", 1, 1, Dir::IN);
	hdl.add_port("i_data", "WIDTH", 1, Dir::IN);
	hdl.add_port("i_mask", "N", 1, Dir::IN);
	hdl.add_port("i_valid", 1, 1, Dir::IN);
	hdl.add_port("o_ready", 1, 1, Dir::OUT);
	hdl.add_port("o_valid", "N", 1, Dir::OUT);
	hdl.add_port("o_data", "WIDTH", 1, Dir::OUT);
	hdl.add_port("i_ready", "N", 1, Dir::IN);
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
	inp->get_endpoint(NET_RS_PHYS, Port::Dir::OUT)->set_max_links(m_n_outputs);

	// Create child ports
	for (unsigned i = 0; i < m_n_outputs; i++)
	{
		auto p = new PortRS(OUTPORT_NAME + std::to_string(i), Port::Dir::OUT, CLOCKPORT_NAME);
		p->add_role_binding(PortRS::VALID, PortBindingRef("o_valid").set_lo_bit(i));
		p->add_role_binding(PortRS::DATA_CARRIER, PortBindingRef("o_data", "WIDTH"));
		p->add_role_binding(PortRS::READY, PortBindingRef("i_ready").set_lo_bit(i));
		p->get_bp_status().make_configurable();
		add_port(p);

		// Connect in->out ports for traversal
		connect(inp, p, NET_RS_PHYS);
	}

	// Create bitmask field with correct size, now that inputs are known
	auto& in_proto = inp->get_proto();
	in_proto.add_terminal_field({ FIELD_SPLITMASK, m_n_outputs }, PortRS::ADDRESS);
}

PortRS * NodeSplit::get_output(unsigned i) const
{
	return get_child_as<PortRS>(OUTPORT_NAME + std::to_string(i));
}

void NodeSplit::prepare_for_hdl()
{
	auto& proto = get_carried_proto();
	set_int_param("WIDTH", proto.get_total_width());
	set_int_param("N", m_n_outputs);
}

PortRS * NodeSplit::get_input() const
{
	return get_child_as<PortRS>(INPORT_NAME);
}