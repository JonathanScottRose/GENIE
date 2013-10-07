#include "merge_node.h"

using namespace ct::P2P;


MergeNode::MergeNode(const std::string& name, const Protocol& proto, int n_inputs)
	: P2P::Node(MERGE), m_proto(proto), m_n_inputs(n_inputs)
{
	set_name(name);

	// Create clock input
	ClockResetPort* cport = new ClockResetPort(Port::CLOCK, Port::IN, this);
	cport->set_name("clock");
	add_port(cport);

	// Create reset input
	cport = new ClockResetPort(Port::RESET, Port::IN, this);
	cport->set_name("reset");
	add_port(cport);
	
	// Tweak protocol
	if (!m_proto.has_field("eop"))
		m_proto.add_field(new Field("eop", 1, Field::FWD));

	// Create the arbiter output port
	DataPort* port = new DataPort(this, Port::OUT);
	port->set_name("out");
	port->set_clock(cport);
	port->set_proto(m_proto);
	add_port(port);

	// Create inports
	for (int i = 0; i < n_inputs; i++)
	{
		port = new DataPort(this, Port::IN);
		port->set_name("in" + std::to_string(i));
		port->set_proto(m_proto);
		port->set_clock(cport);
		add_port(port);
	}
}

MergeNode::~MergeNode()
{
}

DataPort* MergeNode::get_outport()
{
	return (DataPort*)m_ports["out"];
}

DataPort* MergeNode::get_inport(int i)
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

void MergeNode::register_flow(Flow* flow, DataPort* port)
{
	port->add_flow(flow);
	get_outport()->add_flow(flow);
}


