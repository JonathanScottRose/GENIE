#include "ct/merge_node.h"
#include "ct/sys_spec.h"

using namespace ct;
using namespace ct::P2P;
using namespace ct::Spec;

MergeNode::MergeNode(const std::string& name, int n_inputs)
	: P2P::Node(MERGE), m_n_inputs(n_inputs), m_exclusive(false)
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
	proto.init_physfield("xdata", 0);
	
	DataPort* port = new DataPort(Port::OUT, this);
	port->set_name("out");
	port->set_clock(clkport);
	port->set_proto(proto);
	add_port(port);

	// Create inports
	for (int i = 0; i < n_inputs; i++)
	{
		port = new DataPort(Port::IN, this);
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

void MergeNode::configure_1()
{
	Spec::System* sysspec = this->get_parent()->get_spec();

	const auto& links = get_outport()->links();
	
	// See if all Links passing through this merge node are a subset of an exclusion group
	bool all_links_contained = false;

	// Try all groups
	for (const auto& group : sysspec->exclusion_groups())
	{
		// Assume all links belong, and try to contradict
		all_links_contained = true;

		for (auto link : links)
		{
			// Check membership in group by link's label
			const std::string& label = link->get_label();
			if (!Util::exists(group, label))
			{
				all_links_contained = false;
				break;
			}
		}

		// If we managed to get to here, then all links are contained and we're done
		if (all_links_contained)
			break;
	}

	if (all_links_contained)
		this->set_exclusive(true);
}

void MergeNode::carry_fields(const FieldSet& set)
{
	get_outport()->get_proto().carry_fields(set, "xdata");
}

void MergeNode::configure_2()
{
	Protocol& src_proto = get_outport()->get_proto();
	src_proto.allocate_bits();

	for (int i = 0; i < m_n_inputs; i++)
	{
		Port* p = get_inport(i);
		p->get_proto().copy_carriage(src_proto, "xdata");
	}
}