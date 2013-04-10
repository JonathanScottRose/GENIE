#include "at_arb_node.h"
#include "at_manager.h"
#include "ati_arb.h"

ATArbNode::ATArbNode(const std::vector<ATNet*>& nets, const ATLinkProtocol& proto,
					 const std::string& clock)
	: m_proto(proto), m_clock(clock)
{
	m_type = ATNetNode::ARB;

	char buf[128];
	static int s_id = 0;
	sprintf(buf, "arb%d", s_id++);
	m_name = buf;

	// Create the arbiter output port
	ATNetOutPort* new_outport = new ATNetOutPort(this);
	new_outport->set_name("out");
	new_outport->set_clock(clock);
	new_outport->set_proto(proto);
	add_outport(new_outport);

	// Create inport for each net and connect
	int port_no = 0;
	for (ATNet* net : nets)
	{
		ATNetOutPort* driver = net->get_driver();
		ATNetInPort* new_inport = new ATNetInPort(this);
		new_inport->set_clock(m_clock);
		new_inport->set_proto(m_proto);

		sprintf(buf, "in%d", port_no++);
		new_inport->set_name(buf);
		new_inport->add_flows(driver->flows());
		add_inport(new_inport);

		ATNetlist::connect_net_to_inport(net, new_inport);

		// Make sure to add the flows to the outport too
		new_outport->add_flows(driver->flows());
	}
}


ATArbNode::~ATArbNode()
{
}


ATNetOutPort* ATArbNode::get_arb_outport()
{
	return m_outports["out"];
}


void ATArbNode::instantiate()
{
	ATManager* mgr = ATManager::inst();

	int n_inputs = (int)m_inports.size();

	// Instantiate arbiter module
	ati_arb* arb = new ati_arb(sc_gen_unique_name("ati_arb"),
		m_proto, m_proto, n_inputs);
	
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


