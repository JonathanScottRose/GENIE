#include "at_bcast_node.h"
#include "at_manager.h"
#include "ati_bcast.h"

ATBcastNode::ATBcastNode(ATNet* net, const ATLinkProtocol& proto, const std::string& clock)
	: m_proto(proto), m_clock(clock)
{
	m_type = ATNetNode::BCAST;

	static int s_id = 0;
	m_name = "bcast" + std::to_string(s_id++);

	ATNetOutPort* driver = net->get_driver();

	// Create inport
	ATNetInPort* inport = new ATNetInPort(this);
	net->add_fanout(inport);
	inport->set_net(net);
	inport->set_name("in");
	inport->set_clock(m_clock);
	inport->set_proto(m_proto);
	inport->add_flows(driver->flows());
	add_inport(inport);
	
	// Bin flows by destination
	std::map<ATNetInPort*, ATNetFlow*> binned_flows;
	for (ATNetFlow* f : inport->flows())
	{
		binned_flows[f->get_dest_port()] = f;
	}

	// Create outport, one for each physical destination
	int port_no = 0;
	for (auto it : binned_flows)
	{
		ATNetFlow* flow = it.second;
		ATNetOutPort* outport = new ATNetOutPort(this);

		outport->set_name(std::to_string(port_no++));
		outport->set_clock(m_clock);
		outport->set_proto(m_proto);
		outport->add_flow(flow);
		add_outport(outport);

		// Also add this port ot the route map
		m_route_map[flow->get_id()].push_back(outport);
	}
}


ATBcastNode::~ATBcastNode()
{
}


ATNetInPort* ATBcastNode::get_bcast_input()
{
	return m_inports["in"];
}


void ATBcastNode::instantiate()
{
	ATManager* mgr = ATManager::inst();

	// Instantiate broadcast module
	ati_bcast* bcast = new ati_bcast(sc_gen_unique_name("ati_bcast"), m_proto, m_route_map.size());
	
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

