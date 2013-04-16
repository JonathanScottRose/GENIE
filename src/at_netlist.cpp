#include <algorithm>
#include <cassert>
#include <fstream>
#include "at_netlist.h"
#include "at_util.h"
#include "at_instance_node.h"
#include "at_spec.h"

//
// Netlist
//

ATNetlist::ATNetlist()
{
}


ATNetlist::~ATNetlist()
{
	at_delete_all_2(m_nodes);
	
	for (auto i : m_flows)
	{
		at_delete_all(i.second);
	}
}


void ATNetlist::add_node(ATNetNode *node)
{
	assert(!node->get_name().empty());
	assert(m_nodes.find(node->get_name()) == m_nodes.end());
	m_nodes.emplace(node->get_name(), node);
}


void ATNetlist::remove_node(ATNetNode *node)
{
	m_nodes.erase(node->get_name());
}


void ATNetlist::add_flow(ATNetFlow* flow)
{
	m_flows[flow->get_id()].push_back(flow);
}

void ATNetlist::connect_net_to_inport(ATNet* net, ATNetInPort* inport)
{
	assert(std::find(net->fanout().begin(), net->fanout().end(), inport)
		== net->fanout().end());

	assert(inport->get_net() == nullptr);

	net->add_fanout(inport);
	inport->set_net(net);
}

void ATNetlist::connect_outport_to_net(ATNetOutPort* outport, ATNet* net)
{
	assert(net->get_driver() == nullptr);
	assert(outport->get_net() == nullptr);

	net->set_driver(outport);
	outport->set_net(net);
}

ATNetNode* ATNetlist::get_node_for_linkpoint(const ATLinkPointDef& lp_def)
{
	const std::string& inst_name = lp_def.get_inst();
	ATInstanceNode* inst_node = (ATInstanceNode*)get_node(inst_name);
	assert(inst_node->get_type() == ATNetNode::INSTANCE);

	return inst_node;
}

void ATNetlist::dump_graph()
{
	std::ofstream out("graph.dot");

	out << "digraph netlist {" << std::endl;

	for (auto it : m_nodes)
	{
		ATNetNode* src_node = it.second;
		
		for (auto jt : src_node->outports())
		{
			ATNet* net = jt.second->get_net();

			for (auto kt : net->fanout())
			{
				ATNetNode* dest_node = kt->get_parent();
				std::string label;

				for (ATNetFlow* flow : jt.second->flows())
				{
					label += ' ' + std::to_string(flow->get_id());
				}

				out << src_node->get_name() << " -> " << dest_node->get_name() 
					<< " [label=\"" << label << "\"];" << std::endl;
			}
		}
	}

	out << "}" << std::endl;
}

//
// Node
//

ATNetNode::ATNetNode()
{
}


ATNetNode::~ATNetNode()
{
	at_delete_all_2(m_inports);
	at_delete_all_2(m_outports);
}


void ATNetNode::add_inport(ATNetInPort* port)
{
	assert(!port->get_name().empty());
	assert(m_inports.find(port->get_name()) == m_inports.end());
	m_inports.emplace(port->get_name(), port);
}


void ATNetNode::add_outport(ATNetOutPort* port)
{
	assert(!port->get_name().empty());
	assert(m_outports.find(port->get_name()) == m_outports.end());
	m_outports.emplace(port->get_name(), port);
}

//
// Port
//

ATNetPort::ATNetPort(ATNetNode* node)
{
	m_parent = node;
	m_net = nullptr;
}


ATNetPort::~ATNetPort()
{
	if (m_net && m_net->get_driver() == this)
	{
		delete m_net;
	}
}


void ATNetPort::clear_flows()
{
	m_flows.clear();
}


void ATNetPort::add_flow(ATNetFlow* flow)
{
	m_flows.push_back(flow);
}


void ATNetPort::add_flows(const FlowVec& f)
{
	m_flows.insert(m_flows.end(), f.begin(), f.end());
}

//
// Net
//

ATNet::ATNet() { }


ATNet::ATNet(ATNetOutPort* driver)
{
	m_driver = nullptr;
	ATNetlist::connect_outport_to_net(driver, this);
}


void ATNet::add_fanout(ATNetInPort* port)
{
	m_fanout.push_back(port);
}


//
// Flow
//

bool ATNetFlow::same_phys_dest(ATNetFlow* other)
{
	return m_dest_port == other->m_dest_port;
}
