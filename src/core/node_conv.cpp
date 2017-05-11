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
	inport->add_role_binding(PortRS::VALID, "i_valid");
	inport->add_role_binding(PortRS::DATA, { "i_in", "WIDTH_IN" });
	inport->add_role_binding(PortRS::DATA_CARRIER, { "i_data", "WIDTH_DATA" });
	inport->get_bp_status().make_configurable();
	add_port(inport);

	auto outport = new PortRS(OUTPORT_NAME, Port::Dir::OUT, CLOCKPORT_NAME);
	outport->add_role_binding(PortRS::DATA, { "o_out", "WIDTH_OUT" });
	outport->add_role_binding(PortRS::DATA_CARRIER, { "o_data", "WIDTH_DATA" });
	outport->get_bp_status().make_configurable();
	add_port(outport);

	connect(inport, outport, NET_RS_PHYS);
}

NodeConv::NodeConv(const NodeConv& o)
	: Node(o), ProtocolCarrier(o), m_in_width(o.m_in_width), m_out_width(o.m_out_width),
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
	hdl.add_port("i_data", "WIDTH_DATA", 1, Dir::IN);
	hdl.add_port("o_data", "WIDTH_DATA", 1, Dir::OUT);
}

NodeConv* NodeConv::clone() const
{
	return new NodeConv(*this);
}

PortRS * NodeConv::get_input() const
{
	return get_port<PortRS>(INPORT_NAME);
}

PortRS * NodeConv::get_output() const
{
	return get_port<PortRS>(OUTPORT_NAME);
}

void NodeConv::configure(const AddressRep & in_rep, const FieldID & in_field, 
	const AddressRep & out_rep, const FieldID & out_field)
{
	m_table.clear();

	// Go through every output representation address bin. Get the (->to) address.
	// Use one transmission from that bin and find its address in
	// the input representation to use as the (from->) address.
	for (auto& out_bin : out_rep.get_addr_bins())
	{
		unsigned to_addr = out_bin.first;
		unsigned exemplar_xmis = out_bin.second.front();
		unsigned from_addr = in_rep.get_addr(exemplar_xmis);
		m_table.emplace_back(from_addr, to_addr);
	}

	// Set conversion bit sizes
	m_in_width = in_rep.get_size_in_bits();
	m_out_width = out_rep.get_size_in_bits();

	// Attach fields
	auto& in_proto = get_input()->get_proto();
	auto& out_proto = get_output()->get_proto();

	in_proto.add_terminal_field(FieldInst(in_field, m_in_width), PortRS::DATA);
	out_proto.add_terminal_field(FieldInst(out_field, m_out_width), PortRS::DATA);
}

void NodeConv::prepare_for_hdl()
{
	// Fix this later. Might even just bump to 64
	assert(m_in_width <= 32);
	assert(m_out_width <= 32);

	auto& proto = get_carried_proto();
	set_int_param("WIDTH_DATA", proto.get_total_width());

	set_int_param("WIDTH_IN", m_in_width);
	set_int_param("WIDTH_OUT", m_out_width);

	unsigned n_entries = m_table.size();
	set_int_param("N_ENTRIES", n_entries);

	BitsVal in_vals(m_in_width, n_entries);
	BitsVal out_vals(m_out_width, n_entries);

	for (unsigned i = 0; i < n_entries; i++)
	{
		auto& entry = m_table[i];

		in_vals.set_val(0, i, entry.first, m_in_width);
		out_vals.set_val(0, i, entry.second, m_out_width);
	}

	set_bits_param("IN_VALS", in_vals);
	set_bits_param("OUT_VALS", out_vals);
}


