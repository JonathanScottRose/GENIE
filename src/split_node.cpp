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


	// Create inports
	DataPort* port = new DataPort(this, Port::IN);
	port->set_name("in");
	port->set_clock(cport);
	port->set_proto(proto);
	add_port(port);

	// Create outports
	for (int i = 0; i < n_outputs; i++)
	{
		port = new DataPort(this, Port::OUT);
		port->set_name("out" + std::to_string(i));
		port->set_clock(cport);
		port->set_proto(proto);
		add_port(port);
	}
}

SplitNode::~SplitNode()
{
}

void SplitNode::register_flow(Flow* flow, DataPort* port)
{
	get_inport()->add_flow(flow);
	port->add_flow(flow);
	m_route_map[flow->get_id()].push_back(port);
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


/*
void ATBcastNode::instantiate()
{
	ATManager* mgr = ATManager::inst();

	// Instantiate broadcast module
	ati_bcast* bcast = new ati_bcast(sc_gen_unique_name("ati_bcast"), m_proto, m_outports.size());
	
	// Connect clock
	sc_clock* clk = mgr->get_clock(m_clock);
	assert(clk);
	bcast->i_clk(*clk);

	// Attach inport to the module's ati_recv
	ATNetInPort* inport = get_bcast_input();
	assert(inport);

	inport->set_impl(&bcast->i_in);

	// Transfer route map
	for (auto it : m_route_map)
	{
		int flow_id = it.first;
		auto outports = it.second;

		for (ATNetOutPort* outport : outports)
		{
			int out_num = std::stoi(outport->get_name());
			bcast->register_output(flow_id, out_num);
			outport->set_impl(&bcast->o_out(out_num));
		}
	}
}
*/
