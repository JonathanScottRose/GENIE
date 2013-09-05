#include "merge_node.h"

using namespace ct::P2P;


MergeNode::MergeNode(const std::string& name, const Protocol& proto, int n_inputs)
	: P2P::Node(MERGE), m_proto(proto), m_n_inputs(n_inputs)
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
	
	// Tweak protocol


	// Create the arbiter output port
	DataPort* port = new DataPort(this, Port::OUT);
	port->set_name("out");
	port->set_clock(cport);
	port->set_proto(proto);
	add_port(port);

	// Create inports
	for (int i = 0; i < n_inputs; i++)
	{
		port = new DataPort(this, Port::IN);
		port->set_name("in" + std::to_string(i));
		port->set_proto(proto);
		port->set_clock(cport);
		add_port(port);
	}
}

MergeNode::~MergeNode()
{
}

DataPort* MergeNode::get_outport()
{
	return (DataPort*)m_ports["out"];
}

DataPort* MergeNode::get_inport(int i)
{
	return (DataPort*)m_ports["out" + std::to_string(i)];
}

ClockResetPort* MergeNode::get_clock_port()
{
	return (ClockResetPort*)m_ports["clock"];
}

ClockResetPort* MergeNode::get_reset_port()
{
	return (ClockResetPort*)m_ports["reset"];
}

void MergeNode::register_flow(Flow* flow, DataPort* port)
{
	port->add_flow(flow);
	get_outport()->add_flow(flow);
}



/*
void ATArbNode::instantiate()
{
	ATManager* mgr = ATManager::inst();

	int n_inputs = (int)m_inports.size();

	// Instantiate arbiter module
	ati_arb* arb = new ati_arb(m_name.c_str(), m_proto, m_proto, n_inputs);

	// Connect clock
	sc_clock* clk = mgr->get_clock(m_clock);
	assert(clk);
	arb->i_clk(*clk);

	// Attach outport to the module's ati_send
	ATNetOutPort* outport = get_arb_outport();
	assert(outport);

	outport->set_impl(&arb->o_out);

	// Attach inports to the module's ati_recvs, assign addresses
	int i = 0;
	for (auto it : m_inports)
	{
		ATNetInPort* inport = it.second;
		inport->set_impl(&arb->i_in(i++));
	}
}
*/

