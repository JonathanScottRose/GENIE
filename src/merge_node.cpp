#include "merge_node.h"

using namespace ct;
using namespace ct::P2P;

MergeNode::MergeNode(const std::string& name, int n_inputs)
	: P2P::Node(MERGE), m_n_inputs(n_inputs)
{
	set_name(name);

	// Create clock and reset ports
	ClockResetPort* clkport = new ClockResetPort(Port::CLOCK, Port::IN, this);
	clkport->set_name("clock");
	add_port(clkport);

	ClockResetPort* rstport = new ClockResetPort(Port::RESET, Port::IN, this);
	rstport->set_name("reset");
	add_port(rstport);
	
	// Create the arbiter output port
	Protocol proto;
	proto.init_field("valid", 1);
	proto.init_field("ready", 1, PhysField::REV);
	proto.init_field("eop", 1);
	proto.init_physfield("data", 0);
	
	DataPort* port = new DataPort(this, Port::OUT);
	port->set_name("out");
	port->set_clock(clkport);
	port->set_proto(proto);
	add_port(port);

	// Create inports
	for (int i = 0; i < n_inputs; i++)
	{
		port = new DataPort(this, Port::IN);
		port->set_name("in" + std::to_string(i));
		port->set_clock(clkport);
		port->set_proto(proto);
		port->set_aspect("idx", new int(i));
		add_port(port);
	}
}

Port* MergeNode::get_outport()
{
	return (DataPort*)m_ports["out"];
}

Port* MergeNode::get_inport(int i)
{
	return (DataPort*)m_ports["in" + std::to_string(i)];
}

ClockResetPort* MergeNode::get_clock_port()
{
	return (ClockResetPort*)m_ports["clock"];
}

ClockResetPort* MergeNode::get_reset_port()
{
	return (ClockResetPort*)m_ports["reset"];
}

Port* MergeNode::get_free_inport()
{
	for (int i = 0; i < m_n_inputs; i++)
	{
		Port* result = get_inport(i);
		if (result->get_conn() == nullptr)
			return result;
	}
	
	return nullptr;
}

void MergeNode::register_flow(Flow* flow, Port* port)
{
	port->add_flow(flow);
	get_outport()->add_flow(flow);
}

int MergeNode::get_inport_idx(Port* port)
{
	auto val = port->get_aspect<int>("idx");
	assert(val);
	return *val;
}

Node::PortList MergeNode::trace(Port* port, Flow* f)
{
	return PortList(1, get_outport());
}

Port* MergeNode::rtrace(Port* port, Flow* flow)
{
	for (int i = 0; i < m_n_inputs; i++)
	{
		Port* p = get_inport(i);
		if (p->has_flow(flow))
			return p;
	}
	return nullptr;
}

Protocol& MergeNode::get_proto()
{
	return get_outport()->get_proto();
}

void MergeNode::carry_fields(const FieldSet& set)
{
	get_outport()->get_proto().carry_fields(set, "data");
}

void MergeNode::configure_2()
{
	Protocol& src_proto = get_outport()->get_proto();
	src_proto.allocate_bits();

	for (int i = 0; i < m_n_inputs; i++)
	{
		Port* p = get_inport(i);
		p->get_proto().copy_carriage(src_proto, "data");
	}
}