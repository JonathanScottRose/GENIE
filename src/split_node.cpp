#include "split_node.h"

using namespace ct::P2P;


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

	// Protocol
	if (!m_proto.has_field("valid"))
		m_proto.add_field(new Field("valid", 1, Field::FWD));
	if (!m_proto.has_field("ready"))
		m_proto.add_field(new Field("ready", 1, Field::REV));
	if (!m_proto.has_field("flow_id"))
		m_proto.add_field(new Field("flow_id", 0, Field::FWD));

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
		add_port(port);
	}
}

SplitNode::~SplitNode()
{
}

void SplitNode::register_flow(Flow* flow, int outport_idx)
{
	if (!get_inport()->has_flow(flow))
		get_inport()->add_flow(flow);

	get_outport(outport_idx)->add_flow(flow);
	m_route_map[flow->get_id()].push_back(outport_idx);

	int w = get_flow_id_width();
	m_proto.get_field("flow_id")->width = w;
	get_inport()->get_proto().get_field("flow_id")->width = w;
	for (int i = 0; i < m_n_outputs; i++)
	{
		get_outport(i)->get_proto().get_field("flow_id")->width = w;
	}
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

int SplitNode::get_flow_id_width()
{
	int result = 0;
	for (Flow* f : get_inport()->flows())
	{
		result = std::max(result, f->get_id());
	}
	result = Util::log2(result);
	result = std::max(result, 1);
	return result;
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
