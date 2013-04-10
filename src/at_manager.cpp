#include <algorithm>
#include <cstdio>
#include <stack>
#include "at_manager.h"
#include "at_util.h"
#include "sc_bv_signal.h"
#include "at_instance_node.h"
#include "at_arb_node.h"
#include "at_bcast_node.h"
#include "at_netlist.h"


ATManager* ATManager::inst()
{
	static ATManager the_inst;
	return &the_inst;
}


ATManager::ATManager()
{
}


ATManager::~ATManager()
{
}


void ATManager::bind_clock(sc_clock& clk, const std::string& name)
{
	m_clocks[name] = &clk;
}


void ATManager::init_netlist()
{
	int addr = 1;
	int flow_id = 1;

	// Create instance nodes from instance definitions
	for (auto i : m_spec.inst_defs())
	{
		ATInstanceDef* instdef = i.second;
		assert(instdef);

		ATInstanceNode* node = new ATInstanceNode(instdef);
		node->set_addr(addr++);

		m_netlist.add_node(node);
	}

	// Bin flows by source linkpoint
	std::map<std::string, std::vector<ATLinkDef*>> binned_links;
	for (ATLinkDef* linkdef : m_spec.link_defs())
	{
		binned_links[linkdef->src.get_path()].push_back(linkdef);
	}

	// Iterate over bins
	for (auto it : binned_links)
	{
		auto& bin = it.second;

		// Check linkpoint properties
		ATLinkPointDef temp(it.first);
		ATComponentDef* compdef = m_spec.get_component_def_for_instance(temp.get_inst());
		ATEndpointDef* epd = compdef->get_iface(temp.get_iface())->get_endpoint(temp.get_ep());
		bool is_broadcast = epd->type == ATEndpointDef::BROADCAST;

		for (ATLinkDef* linkdef : bin)
		{
			ATNetFlow* flow = new ATNetFlow;
			flow->set_id(flow_id);
			flow->set_def(linkdef);
		
			ATNetNode* s_node = m_netlist.get_node(linkdef->src.get_inst());
			ATNetNode* d_node = m_netlist.get_node(linkdef->dest.get_inst());
			assert(s_node);
			assert(d_node);

			ATNetOutPort* s_port = s_node->get_outport(linkdef->src.get_iface());
			ATNetInPort* d_port = d_node->get_inport(linkdef->dest.get_iface());
			assert(s_port);
			assert(d_port);

			s_port->add_flow(flow);
			d_port->add_flow(flow);
			flow->set_src_port(s_port);
			flow->set_dest_port(d_port);

			// Flows that are part of a broadcast group share flow_id
			if (!is_broadcast)
				flow_id++;
		}
	}
}


void ATManager::create_arb_bcast()
{
	std::stack<ATNetOutPort*> to_visit;

	// Initialize stack with all outports of instance nodes
	for (auto it : m_netlist.nodes())
	{
		ATNetNode* node = it.second;
		if (node->get_type() != ATNetNode::INSTANCE)
			continue;

		for (auto it : node->outports())
		{
			ATNetOutPort* port = it.second;
			to_visit.push(port);
		}
	}

	// For every instance node's inport, this holds all the outports
	// competing for that inport. This will be used to generate ARBs.
	std::map<ATNetInPort*, std::vector<ATNetOutPort*>> done_pile;

	// Visit all the outports and instantiate broadcast nodes
	while (!to_visit.empty())
	{
		ATNetOutPort* port = to_visit.top();
		to_visit.pop();

		// Get the list of flows traveling through this outport
		auto& flows = port->flows();
		if (flows.empty())
			throw std::runtime_error("Unconnected port");

		// See if all the flows have the same physical destination or not
		bool all_same_dest = true;
		std::for_each(flows.begin(), flows.end(), [&](ATNetFlow* f)
		{
			all_same_dest &= f->same_phys_dest(flows.front());
		});

		if (all_same_dest)
		{
			// The common destination of all the flows
			ATNetInPort* final_dest = flows.front()->get_dest_port();
			
			// The outport now competes with potentially others for this destination
			done_pile[final_dest].push_back(port);
		}
		else
		{
			// Create a broadcast node
			ATNet* net = new ATNet(port);

			ATBcastNode* bc = new ATBcastNode(net, port->get_proto(), port->get_clock());
			m_netlist.add_node(bc);

			// Push its outports onto visitation stack
			for (auto it : bc->outports())
			{
				to_visit.push(it.second);
			}
		}
	}

	// Now visit the done pile and create arbiters
	for (auto i : done_pile)
	{
		// The destination inport port...
		ATNetInPort* dest_inport = i.first;

		// ... which all of these outports compete for
		auto& src_outports = i.second;

		if (src_outports.size() > 1)
		{
			// Create a net for each outport to drive, collect these nets
			std::vector<ATNet*> src_nets;
			for (ATNetOutPort* src_outport : src_outports)
			{
				ATNet* net = new ATNet(src_outport);
				src_nets.push_back(net);
			}

			// Create an arbiter node and give it all these nets as inputs
			ATArbNode* arb = new ATArbNode(src_nets, dest_inport->get_proto(), dest_inport->get_clock());
			m_netlist.add_node(arb);

			// Connect the output of the arb node to its final destination via a net
			ATNetOutPort* arb_outport = arb->get_arb_outport();
			ATNet* net = new ATNet(arb_outport);
			ATNetlist::connect_net_to_inport(net, dest_inport);
		}
		else
		{
			ATNetOutPort* the_outport = src_outports.front();
			ATNet* net = new ATNet(the_outport);
			ATNetlist::connect_net_to_inport(net, dest_inport);
		}
	}
}


void ATManager::build_system()
{
	m_spec.validate_and_preprocess();

	init_netlist();
	create_arb_bcast();

	// Instantiate every node in the netlist
	for (auto it : m_netlist.nodes())
	{
		ATNetNode* node = it.second;
		node->instantiate();
	}

	// Make connections between nodes
	for (auto it : m_netlist.nodes())
	{
		ATNetNode* src_node = it.second;

		// For each outport, connect to its fanout
		for (auto it : src_node->outports())
		{
			ATNetOutPort* src_outport = it.second;
			ati_send* send = src_outport->get_impl();

			for (ATNetInPort* dest_inport : src_outport->get_net()->fanout())
			{
				ati_recv* recv = dest_inport->get_impl();

				assert(dest_inport->get_proto().compatible_with(src_outport->get_proto()));

				ati_channel* chan = new ati_channel(src_outport->get_proto());
				send->bind(*chan);
				recv->bind(*chan);
			}
		}
	}
}

