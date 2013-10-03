#include "flow_conv_node.h"

using namespace ct::P2P;

FlowConvNode::FlowConvNode(const std::string& name, bool to_flow, const Protocol& user_proto,
						   const DataPort::Flows& flows, DataPort* user_port)
	: P2P::Node(FLOW_CONV), m_to_flow(to_flow), m_user_port(user_port)
{
	set_name(name);

	// Create clock input
	ClockResetPort* cport = new ClockResetPort(Port::CLOCK, Port::IN, this);
	cport->set_name("clock");
	add_port(cport);

	// Create reset input
	ClockResetPort* rport = new ClockResetPort(Port::RESET, Port::IN, this);
	rport->set_name("reset");
	add_port(rport);

	// Create input
	DataPort* in_port = new DataPort(this, Port::IN);
	in_port->set_name("in");
	in_port->set_clock(cport);
	in_port->add_flows(flows);
	add_port(in_port);

	// Create output
	DataPort* out_port = new DataPort(this, Port::OUT);
	out_port->set_name("out");
	out_port->set_clock(cport);
	out_port->add_flows(flows);
	add_port(out_port);

	DataPort* user_facing_port = to_flow? in_port : out_port;
	DataPort* sys_facing_port = to_flow? out_port : in_port;

	// Configure user port
	user_facing_port->set_proto(user_proto);

	// Calculate flow_id width
	int flow_width = 0;
	for (Flow* f : flows)
	{
		flow_width = std::max(flow_width, f->get_id());
	}
	flow_width = Util::log2(flow_width);

	// Configure system port
	Protocol sys_proto = user_proto;
	assert(!sys_proto.has_field("link_id"));
	sys_proto.delete_field("lp_id");
	sys_proto.add_field(new Field("flow_id", flow_width, Field::FWD));
	sys_facing_port->set_proto(sys_proto);
}

FlowConvNode::~FlowConvNode()
{
}

ClockResetPort* FlowConvNode::get_clock_port()
{
	return (ClockResetPort*)get_port("clock");
}

ClockResetPort* FlowConvNode::get_reset_port()
{
	return (ClockResetPort*)get_port("reset");
}

DataPort* FlowConvNode::get_inport()
{
	return (DataPort*)get_port("in");
}

DataPort* FlowConvNode::get_outport()
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
	const Protocol& proto = get_inport()->get_proto();
	Field* f = proto.get_field(get_in_field_name());
	return f->width;
}

int FlowConvNode::get_out_field_width()
{
	const Protocol& proto = get_outport()->get_proto();
	Field* f = proto.get_field(get_out_field_name());
	return f->width;
}

const DataPort::Flows& FlowConvNode::get_flows()
{
	return get_inport()->flows();
}


