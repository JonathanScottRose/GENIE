#include <unordered_set>
#include "ct/split_node.h"


using namespace ct;
using namespace ct::P2P;


SplitNode::SplitNode(const std::string& name, int n_outputs)
	: Node(SPLIT), m_n_outputs(n_outputs)
{
	set_name(name);

	// Create clock and reset ports
	ClockResetPort* clkport = new ClockResetPort(Port::CLOCK, Port::IN, this);
	clkport->set_name("clock");
	add_port(clkport);

	ClockResetPort* rstport = new ClockResetPort(Port::RESET, Port::IN, this);
	rstport->set_name("reset");
	add_port(rstport);

	// Create inports
	Protocol proto;
	proto.init_field("valid", 1);
	proto.init_field("ready", 1, PhysField::REV);
	proto.init_physfield("xdata", 0);

	DataPort* port = new DataPort(this, Port::IN);
	port->set_name("in");
	port->set_clock(clkport);
	port->set_proto(proto);
	add_port(port);

	// Create outports
	for (int i = 0; i < n_outputs; i++)
	{
		port = new DataPort(this, Port::OUT);
		port->set_name("out" + std::to_string(i));
		port->set_clock(clkport);
		port->set_proto(proto);
		port->set_aspect_val("idx", i);
		add_port(port);
	}
}

void SplitNode::register_flow(Flow* flow, int outport_idx)
{
	if (!get_inport()->has_flow(flow))
		get_inport()->add_flow(flow);

	get_outport(outport_idx)->add_flow(flow);
	m_route_map[flow->get_id()].push_back(outport_idx);
}

Port* SplitNode::get_inport()
{
	return (DataPort*)get_port("in");
}

Port* SplitNode::get_outport(int i)
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
	return port->get_aspect_val<int>("idx");
}

Port* SplitNode::get_free_outport()
{
	for (int i = 0; i < m_n_outputs; i++)
	{
		Port* result = get_outport(i);
		if (result->get_conn() == nullptr)
			return result;
	}
	
	return nullptr;
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

Node::PortList SplitNode::trace(Port* in, Flow* f)
{
	PortList result;

	for (int i = 0; i < m_n_outputs; i++)
	{
		Port* out = get_outport(i);
		if (out->has_flow(f))
			result.push_back(out);
	}

	return result;
}

Port* SplitNode::rtrace(Port* port, Flow* flow)
{
	return get_inport();
}

void SplitNode::configure_1()
{
	// add flow_id fields to protocol
	get_inport()->get_proto().init_field("flow_id", m_parent->get_global_flow_id_width());

	for (int i = 0; i < m_n_outputs; i++)
	{
		get_outport(i)->get_proto().init_field(
			"flow_id", m_parent->get_global_flow_id_width());
	}

	// update route map (could be obsolete later)
	for (auto flow : get_inport()->flows())
	{
		for (int i = 0; i < m_n_outputs; i++)
		{
			Port* out = get_outport(i);
			if (out->has_flow(flow))
			{
				m_route_map[flow->get_id()].push_back(i);
			}
		}
	}
}

Protocol& SplitNode::get_proto()
{
	return get_inport()->get_proto();
}

void SplitNode::carry_fields(const FieldSet& set)
{
	get_inport()->get_proto().carry_fields(set, "xdata");
}

void SplitNode::configure_2()
{
	Protocol& src_proto = get_inport()->get_proto();
	src_proto.allocate_bits();

	for (int i = 0; i < m_n_outputs; i++)
	{
		Port* p = get_outport(i);
		p->get_proto().copy_carriage(src_proto, "xdata");
	}
}