#include "flow_conv_node.h"

using namespace ct::P2P;

FlowConvNode::FlowConvNode(const std::string& name, bool to_flow, const Protocol& user_proto,
						   const DataPort::Flows& flows)
	: P2P::Node(FLOW_CONV)
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

	// Create input
	DataPort* in_port = new DataPort(this, Port::IN);
	in_port->set_name("in");
	in_port->set_clock(cport);
	add_port(in_port);

	// Create output
	DataPort* out_port = new DataPort(this, Port::OUT);
	out_port->set_name("out");
	out_port->set_clock(cport);
	add_port(out_port);

	DataPort* user_port = to_flow? in_port : out_port;
	DataPort* sys_port = to_flow? out_port : in_port;

	// Configure user port
	user_port->set_proto(user_proto);

	// Configure system port
	Protocol sys_proto = user_proto;
	int flow_width = Util::log2(flows.size());
	assert(!sys_proto.has_field("link_id"));
	sys_proto.delete_field("lp_id");
	sys_proto.add_field(new Field("flow_id", flow_width, Field::FWD));
	sys_port->set_proto(sys_proto);
}