#include "ct/flow_conv_node.h"

using namespace ct::P2P;

FlowConvNode::FlowConvNode(const std::string& name, bool to_flow)
	: Node(FLOW_CONV), m_to_flow(to_flow)
{
	set_name(name);

	// Create clock and reset ports
	ClockResetPort* clkport = new ClockResetPort(Port::CLOCK, Port::IN, this);
	clkport->set_name("clock");
	add_port(clkport);

	ClockResetPort* rstport = new ClockResetPort(Port::RESET, Port::IN, this);
	rstport->set_name("reset");
	add_port(rstport);

	// Common protocol for input and output (flow_id and lp_id created in configure())
	Protocol proto;
	proto.init_physfield("xdata", 0);
	proto.init_field("valid", 1);
	proto.init_field("ready", 1, PhysField::REV);

	// Create input
	DataPort* in_port = new DataPort(this, Port::IN);
	in_port->set_name("in");
	in_port->set_clock(clkport);
	in_port->set_proto(proto);
	add_port(in_port);

	// Create output
	DataPort* out_port = new DataPort(this, Port::OUT);
	out_port->set_name("out");
	out_port->set_clock(clkport);
	out_port->set_proto(proto);
	add_port(out_port);
}

ClockResetPort* FlowConvNode::get_clock_port()
{
	return (ClockResetPort*)get_port("clock");
}

ClockResetPort* FlowConvNode::get_reset_port()
{
	return (ClockResetPort*)get_port("reset");
}

Port* FlowConvNode::get_inport()
{
	return (DataPort*)get_port("in");
}

Port* FlowConvNode::get_outport()
{
	return (DataPort*)get_port("out");
}

const std::string& FlowConvNode::get_in_field_name()
{
	static std::string nm1 = "flow_id";
	static std::string nm2 = "lp_id";
	return m_to_flow? nm2 : nm1;
}

const std::string& FlowConvNode::get_out_field_name()
{
	static std::string nm1 = "flow_id";
	static std::string nm2 = "lp_id";
	return m_to_flow? nm1 : nm2;
}

int FlowConvNode::get_n_entries()
{
	return get_inport()->flows().size();
}

int FlowConvNode::get_in_field_width()
{
	Protocol& proto = get_inport()->get_proto();
	Field* f = proto.get_field(get_in_field_name());
	return f->width;
}

int FlowConvNode::get_out_field_width()
{
	Protocol& proto = get_outport()->get_proto();
	Field* f = proto.get_field(get_out_field_name());
	return f->width;
}

const Port::Flows& FlowConvNode::get_flows()
{
	return get_inport()->flows();
}

Node::PortList FlowConvNode::trace(Port* port, Flow* flow)
{
	return PortList(1, get_outport());
}

Port* FlowConvNode::rtrace(Port* port, Flow* flow)
{
	return get_inport();
}

void FlowConvNode::configure_1()
{
	// set correct lp_id width and flow_id width
	Port* user_facing_port = m_to_flow ? get_inport() : get_outport();
	Port* sys_facing_port = m_to_flow ? get_outport() : get_inport();

	Port* user_port = user_facing_port->get_first_connected_port();
	int lp_id_width = user_port->get_proto().get_field("lp_id")->width;
	user_facing_port->get_proto().init_field("lp_id", lp_id_width);
	
	sys_facing_port->get_proto().init_field("flow_id", m_parent->get_global_flow_id_width());

	m_user_port = user_facing_port->get_first_connected_port();
}

void FlowConvNode::carry_fields(const FieldSet& set)
{
	get_outport()->get_proto().carry_fields(set, "xdata");
}

void FlowConvNode::configure_2()
{
	get_outport()->get_proto().allocate_bits();
	get_inport()->get_proto().copy_carriage(get_outport()->get_proto(),	"xdata");
}