#include "pch.h"
#include "node_merge.h"
#include "net_topo.h"
#include "net_rs.h"
#include "port_clockreset.h"
#include "port_rs.h"
#include "genie_priv.h"

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
}

void NodeMerge::init()
{
	genie::impl::register_reserved_module(MODNAME);
	genie::impl::register_reserved_module(MODNAME_EX);
}

NodeMerge::NodeMerge()
    : Node(MODNAME, MODNAME), m_n_inputs(0)
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
	add_port(outport);

	auto& proto = outport->get_proto();
	proto.add_terminal_field({ FIELD_EOP, 1 }, PortRS::EOP);

	// The node itself can be connected to with TOPO links
	make_connectable(NET_TOPO);
	get_endpoint(NET_TOPO, Port::Dir::IN)->set_max_links(Endpoint::UNLIMITED);
}

NodeMerge::NodeMerge(const NodeMerge& o)
    : Node(o)
{	
    // Create a copy of an existing NodeMerge
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

	// Create child ports
	for (unsigned i = 0; i < m_n_inputs; i++)
	{
		auto p = new PortRS(INPORT_NAME + std::to_string(i), Port::Dir::IN, CLOCKPORT_NAME);
		p->add_role_binding(PortRS::VALID, PortBindingRef("i_valid").set_lo_bit(i));
		p->add_role_binding(PortRS::DATA_CARRIER,
			PortBindingRef("i_data", "WIDTH").set_lo_slice(i));
		p->add_role_binding(PortRS::EOP, PortBindingRef("i_eop").set_lo_bit(i));
		p->add_role_binding(PortRS::READY, PortBindingRef("o_ready").set_lo_bit(i));
		add_port(p);

		auto& proto = p->get_proto();
		proto.add_terminal_field({ FIELD_EOP, 1 }, PortRS::EOP);

		// Connect to output port, for traversal
		connect(p, outp, NET_RS);
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
