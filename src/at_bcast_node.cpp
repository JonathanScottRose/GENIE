#include "at_bcast_node.h"
#include "at_manager.h"
#include "ati_bcast.h"

ATBcastNode::ATBcastNode(int n_outputs, const ATLinkProtocol &in_proto, 
						 const ATLinkProtocol &out_proto, const std::string& clock)
: m_in_proto(in_proto), m_out_proto(out_proto), m_n_outputs(n_outputs), m_clock(clock)
{
	m_type = ATNetNode::BCAST;

	for (int i = 0; i < n_outputs; i++)
	{
		ATNetOutPort* outport = new ATNetOutPort(this);
		outport->set_proto(m_out_proto);
		outport->set_clock(clock);
		m_outports.push_back(outport);
	}

	ATNetInPort* inport = new ATNetInPort(this);
	inport->set_proto(m_in_proto);
	inport->set_clock(clock);
	m_inports.push_back(inport);

	m_addr_map.resize(n_outputs);
}


ATBcastNode::~ATBcastNode()
{
}


void ATBcastNode::instantiate()
{
	ATManager* mgr = ATManager::inst();

	// Instantiate broadcast module
	ati_bcast* bcast = new ati_bcast(sc_gen_unique_name("ati_bcast"), 
		m_in_proto, m_out_proto, m_n_outputs);
	
	// Connect clock
	sc_clock* clk = mgr->get_clock(m_clock);
	assert(clk);
	bcast->i_clk(*clk);

	// Attach inport to the module's ati_recv
	ATNetInPort* inport = m_inports.front();
	assert(inport);

	inport->set_impl(&bcast->i_in);

	// Attach outports to the module's ati_sends, assign addresses
	for (int i = 0; i < m_n_outputs; i++)
	{
		ATNetOutPort* outport = m_outports[i];
		outport->set_impl(&bcast->o_out(i));

		sc_bv_base addr(m_in_proto.addr_width);
		addr = m_addr_map[i];
		bcast->set_addr(i, &addr);
	}
}


void ATBcastNode::set_addr(int port, int addr)
{
	m_addr_map[port] = addr;
}
