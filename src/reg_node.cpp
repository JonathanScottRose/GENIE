#include "ct/reg_node.h"

using namespace ct::P2P;

RegNode::RegNode(const std::string& name)
: Node(REG_STAGE)
{
	set_name(name);

	// Create clock and reset ports
	ClockResetPort* clkport = new ClockResetPort(Port::CLOCK, Port::IN, this);
	clkport->set_name("clock");
	add_port(clkport);

	ClockResetPort* rstport = new ClockResetPort(Port::RESET, Port::IN, this);
	rstport->set_name("reset");
	add_port(rstport);

	// Common protocol for input and output
	Protocol proto;
	proto.init_physfield("xdata", 0);
	proto.init_field("valid", 1);
	proto.init_field("ready", 1, PhysField::REV);

	// Create input
	DataPort* in_port = new DataPort(Port::IN, this);
	in_port->set_name("in");
	in_port->set_clock(clkport);
	in_port->set_proto(proto);
	add_port(in_port);

	// Create output
	DataPort* out_port = new DataPort(Port::OUT, this);
	out_port->set_name("out");
	out_port->set_clock(clkport);
	out_port->set_proto(proto);
	add_port(out_port);
}

ClockResetPort* RegNode::get_clock_port()
{
	return (ClockResetPort*)get_port("clock");
}

ClockResetPort* RegNode::get_reset_port()
{
	return (ClockResetPort*)get_port("reset");
}

DataPort* RegNode::get_inport()
{
	return (DataPort*)get_port("in");
}

DataPort* RegNode::get_outport()
{
	return (DataPort*)get_port("out");
}

Node::PortList RegNode::trace(Port* port, Flow* flow)
{
	return PortList(1, get_outport());
}

Port* RegNode::rtrace(Port* port, Flow* flow)
{
	return get_inport();
}

void RegNode::configure_1()
{
}

void RegNode::carry_fields(const FieldSet& set)
{
	get_outport()->get_proto().carry_fields(set, "xdata");
}

void RegNode::configure_2()
{
	get_outport()->get_proto().allocate_bits();
	get_inport()->get_proto().copy_carriage(get_outport()->get_proto(), "xdata");
}