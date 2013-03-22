#include "at_arb_node.h"
#include "at_manager.h"
#include "ati_arb.h"

ATArbNode::ATArbNode(int n_inputs, const ATLinkProtocol &in_proto, const ATLinkProtocol &out_proto,
					 const std::string& clock)
: m_in_proto(in_proto), m_out_proto(out_proto), m_n_inputs(n_inputs), m_clock(clock)
{
	m_type = ATNetNode::ARB;

	m_in_proto.is_packet = true;
	m_out_proto.has_ready = true;

	for (int i = 0; i < n_inputs; i++)
	{
		ATNetInPort* inport = new ATNetInPort(this);
		inport->set_proto(m_in_proto);
		inport->set_clock(clock);
		m_inports.push_back(inport);
	}

	ATNetOutPort* outport = new ATNetOutPort(this);
	outport->set_proto(m_out_proto);
	outport->set_clock(clock);
	m_outports.push_back(outport);

	m_addr_map.resize(n_inputs);
}


ATArbNode::~ATArbNode()
{
}


void ATArbNode::instantiate()
{
	ATManager* mgr = ATManager::inst();

	// Instantiate arbiter module
	ati_arb* arb = new ati_arb(sc_gen_unique_name("ati_arb"),
		m_in_proto, m_out_proto, m_n_inputs);
	
	// Connect clock
	sc_clock* clk = mgr->get_clock(m_clock);
	assert(clk);
	arb->i_clk(*clk);

	// Attach outport to the module's ati_send
	ATNetOutPort* outport = m_outports.front();
	assert(outport);

	outport->set_impl(&arb->o_out);

	// Attach inports to the module's ati_recvs, assign addresses
	for (int i = 0; i < m_n_inputs; i++)
	{
		ATNetInPort* inport = m_inports[i];
		inport->set_impl(&arb->i_in(i));

		sc_bv_base addr(m_out_proto.addr_width);
		addr = m_addr_map[i];
		arb->set_addr(i, &addr);
	}
}


void ATArbNode::set_addr(int port, int addr)
{
	m_addr_map[port] = addr;
}
