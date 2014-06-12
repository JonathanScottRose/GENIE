#include "ct/clock_cross_node.h"

using namespace ct::P2P;

ClockCrossNode::ClockCrossNode(const std::string& name)
: Node(CLOCK_CROSS)
{
	set_name(name);

	// Create reset port
	ClockResetPort* rstport = new ClockResetPort(Port::RESET, Port::IN, this);
	rstport->set_name("reset");
	add_port(rstport);

	// Create protocol for input/output data ports
	Protocol proto;
	proto.init_physfield("xdata", 0);
	proto.init_field("valid", 1);
	proto.init_field("ready", 1, PhysField::REV);

	// Create input side
	ClockResetPort* clockinport = new ClockResetPort(Port::CLOCK, Port::IN, this);
	clockinport->set_name("clock_in");
	add_port(clockinport);
	
	DataPort* datainport = new DataPort(this, Port::IN);
	datainport->set_name("data_in");
	datainport->set_clock(clockinport);
	datainport->set_proto(proto);
	add_port(datainport);

	// Create output side
	ClockResetPort* clockoutport = new ClockResetPort(Port::CLOCK, Port::IN, this);
	clockoutport->set_name("clock_out");
	add_port(clockoutport);

	DataPort* dataoutport = new DataPort(this, Port::OUT);
	dataoutport->set_name("data_out");
	dataoutport->set_clock(clockoutport);
	dataoutport->set_proto(proto);
	add_port(dataoutport);
}

ClockResetPort* ClockCrossNode::get_reset_port()
{
	return (ClockResetPort*)get_port("reset");
}

Port* ClockCrossNode::get_inport()
{
	return get_port("data_in");
}

Port* ClockCrossNode::get_outport()
{
	return get_port("data_out");
}

ClockResetPort* ClockCrossNode::get_inclock_port()
{
	return (ClockResetPort*)get_port("clock_in");
}

ClockResetPort* ClockCrossNode::get_outclock_port()
{
	return (ClockResetPort*)get_port("clock_out");
}

Node::PortList ClockCrossNode::trace(Port* port, Flow* flow)
{
	return PortList(1, get_outport());
}

Port* ClockCrossNode::rtrace(Port* port, Flow* flow)
{
	return get_inport();
}

void ClockCrossNode::configure_1()
{
}

void ClockCrossNode::carry_fields(const FieldSet& set)
{
	get_outport()->get_proto().carry_fields(set, "xdata");
}

void ClockCrossNode::configure_2()
{
	get_outport()->get_proto().allocate_bits();
	get_inport()->get_proto().copy_carriage(get_outport()->get_proto(), "xdata");
}