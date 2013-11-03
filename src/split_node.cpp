#include "split_node.h"

using namespace ct;
using namespace ct::P2P;

namespace
{
	// Custom impl-aspect for outports: holds integer index of outport
	struct PortIndex : public ImplAspect
	{
		PortIndex(int i) : idx(i) {}
		int idx;
	};
}

SplitNode::SplitNode(const std::string& name, const Protocol& proto, int n_outputs)
	: Node(SPLIT), m_n_outputs(n_outputs), m_proto(proto)
{
	set_name(name);

	// Create clock and reset ports
	ClockResetPort* cport = new ClockResetPort(Port::CLOCK, Port::IN, this);
	cport->set_name("clock");
	add_port(cport);

	cport = new ClockResetPort(Port::RESET, Port::IN, this);
	cport->set_name("reset");
	add_port(cport);

	// Force required fields in the protocol (hack, please fix me).
	// The input protocol may be lacking a flow_id if it's connected to a
	// broadcast linkpoint, so we have to add this field in the input, figure
	// out its width based on the flow coming in, and then let allow that
	// field to be defaulted
	if (!m_proto.has_field("valid"))
		m_proto.add_field(new Field("valid", 1, Field::FWD));
	if (!m_proto.has_field("ready"))
		m_proto.add_field(new Field("ready", 1, Field::REV));
	if (!m_proto.has_field("flow_id"))
		m_proto.add_field(new Field("flow_id", 1, Field::FWD)); // configured later to correct width

	// Create inports
	DataPort* port = new DataPort(this, Port::IN);
	port->set_name("in");
	port->set_clock(cport);
	port->set_proto(m_proto);
	add_port(port);

	// Create outports
	for (int i = 0; i < n_outputs; i++)
	{
		port = new DataPort(this, Port::OUT);
		port->set_name("out" + std::to_string(i));
		port->set_clock(cport);
		port->set_proto(m_proto);
		port->set_impl("idx", new PortIndex(i));
		add_port(port);
	}
}

SplitNode::~SplitNode()
{
}

void SplitNode::configure()
{
	// After all the flows have been added, figure out the width of the flow_id field.
	// This is duplicating work if the protocol already had a properly-sized flow_id field,
	// but is necessary if we're creating a defaulted flow_id field at the input.
	int w = get_flow_id_width();

	// There's 3 copies of the protocol lying around here...
	m_proto.get_field("flow_id")->width = w;

	get_inport()->get_proto().get_field("flow_id")->width = w;

	for (int i = 0; i < m_n_outputs; i++)
	{
		get_outport(i)->get_proto().get_field("flow_id")->width = w;
	}
}

void SplitNode::register_flow(Flow* flow, int outport_idx)
{
	if (!get_inport()->has_flow(flow))
		get_inport()->add_flow(flow);

	get_outport(outport_idx)->add_flow(flow);
	m_route_map[flow->get_id()].push_back(outport_idx);
}

DataPort* SplitNode::get_inport()
{
	return (DataPort*)get_port("in");
}

DataPort* SplitNode::get_outport(int i)
{
	return (DataPort*)get_port("out" + std::to_string(i));
}

ClockResetPort* SplitNode::get_clock_port()
{
	return (ClockResetPort*)get_port("clock");
}

ClockResetPort* SplitNode::get_reset_port()
{
	return (ClockResetPort*)get_port("reset");
}

int SplitNode::get_n_flows()
{
	return get_inport()->flows().size();
}

int SplitNode::get_idx_for_outport(Port* port)
{
	auto impl = (PortIndex*)port->get_impl("idx");
	assert(impl);
	return impl->idx;
}

int SplitNode::get_flow_id_width()
{
	// FIXME: make more efficient to reduce width
	return m_parent->get_global_flow_id_width();
}

auto SplitNode::get_dests_for_flow(int flow_id) -> const DestVec&
{
	assert(m_route_map.count(flow_id) > 0);
	return m_route_map[flow_id];
}

const DataPort::Flows& SplitNode::get_flows()
{
	return get_inport()->flows();
}
