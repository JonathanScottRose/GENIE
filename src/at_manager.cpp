#include <algorithm>
#include <cstdio>
#include "at_manager.h"
#include "at_util.h"
#include "sc_bv_signal.h"
#include "at_instance_node.h"
#include "at_arb_node.h"
#include "at_bcast_node.h"
#include "at_adapter_node.h"
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

class ATPath
{
public:
	ATPath(const std::string& path)
	{
		std::stringstream strm(path);
		std::getline(strm, m_inst, '.');
		std::getline(strm, m_iface, '.');
	}

	const std::string& get_inst() { return m_inst; }
	const std::string& get_iface() { return m_iface; }

private:
	std::string m_inst;
	std::string m_iface;
};


void ATManager::bind_clock(sc_clock& clk, const std::string& name)
{
	m_clocks[name] = &clk;
}


void ATManager::build_netlist()
{
	int addr = 1;

	// Create instance nodes from instance definitions
	for (auto i : m_spec.inst_defs())
	{
		ATInstanceDef* instdef = i.second;
		assert(instdef);

		ATInstanceNode* node = new ATInstanceNode(instdef);
		m_netlist.add_node(node);
	}

	// Connect instance nodes together according to link definitions
	for (ATLinkDef* linkdef : m_spec.link_defs())
	{
		assert(linkdef);

		ATPath s_path(linkdef->src);
		ATPath d_path(linkdef->dest);
		
		ATInstanceNode* s_node = m_netlist.get_instance_node(s_path.get_inst());
		ATInstanceNode* d_node = m_netlist.get_instance_node(d_path.get_inst());
		assert(s_node);
		assert(d_node);

		ATNetOutPort* s_port = s_node->get_port_for_send(s_path.get_iface());
		ATNetInPort* d_port = d_node->get_port_for_recv(d_path.get_iface());
		assert(s_port);
		assert(d_port);

		s_port->get_fanout().push_back(d_port);
		d_port->get_fanin().push_back(s_port);
	}
}


void ATManager::xform_arbs()
{
	ATNetlist::NodeVec to_add;

	// For each node, find any inports with multiple fanins and insert arbiters
	for (ATNetNode* dest_node : m_netlist.nodes())
	{
		ATNetNode::InPortVec& dest_inports = dest_node->get_inports();

		for (ATNetInPort* dest_inport : dest_inports)
		{
			ATNetInPort::FaninVec& dest_fanin = dest_inport->get_fanin();
			int n_fanin = (int)dest_fanin.size();

			// Ignore single fanins
			if (n_fanin <= 1)
				continue;
			
			// Only instances can have addresses for now
			assert(dest_node->get_type() == ATNetNode::INSTANCE);

			// Instantiate arb node
			const ATLinkProtocol& src_proto = dest_fanin.front()->get_proto();
			const ATLinkProtocol& dest_proto = dest_inport->get_proto();
			const std::string& dest_clock = dest_inport->get_clock();

			ATArbNode* arb_node = new ATArbNode(n_fanin, 
				src_proto, dest_proto, dest_clock);

			to_add.push_back(arb_node);

			// Rewire netlist to include arb node
			for (int j = 0; j < n_fanin; j++)
			{
				ATNetInPort* arb_inport = arb_node->get_inports().at(j);

				// Outport that feeds dest_node's fanin
				ATNetOutPort* src_outport = dest_fanin[j];
				ATNetOutPort::FanoutVec& src_fanout = src_outport->get_fanout();
				
				// Disconnect dest node from src node, replace with connection to arbiter
				ATNetOutPort::FanoutVec::iterator k = 
					std::find(src_fanout.begin(), src_fanout.end(),	dest_inport);

				// Connect upstream node to arbiter
				*k = arb_inport;
				arb_inport->get_fanin().push_back(src_outport);

				// Inform arbiter of the address associated with this input
				arb_node->set_addr(j, j);
			}

			// Connect arbiter's output to dest_node
			ATNetOutPort* arb_outport = arb_node->get_outports().front();
			dest_fanin.clear();
			dest_fanin.push_back(arb_outport);
			arb_outport->get_fanout().push_back(dest_inport);
		}
	}

	for (auto i : to_add)
	{
		m_netlist.add_node(i);
	}
}


void ATManager::xform_bcasts()
{
	ATNetlist::NodeVec to_add;

	// For each node, find any outports with multiple fanouts and insert broadcasters
	for (ATNetNode* src_node : m_netlist.nodes())
	{
		ATNetNode::OutPortVec& src_outports = src_node->get_outports();

		for (ATNetOutPort* src_outport : src_outports)
		{
			ATNetOutPort::FanoutVec& src_fanout = src_outport->get_fanout();
			int n_fanout = (int)src_fanout.size();

			// Ignore single fanouts
			if (n_fanout <= 1)
				continue;
			
			// Only instances can have addresses for now
			assert(src_node->get_type() == ATNetNode::INSTANCE);

			// Instantiate bcast node
			const ATLinkProtocol& dest_proto = src_fanout.front()->get_proto();
			const ATLinkProtocol& src_proto = src_outport->get_proto();
			const std::string& src_clock = src_outport->get_clock();

			ATBcastNode* bcast_node = new ATBcastNode(n_fanout, 
				src_proto, dest_proto, src_clock);

			to_add.push_back(bcast_node);

			// Rewire netlist to include bcast node
			for (int j = 0; j < n_fanout; j++)
			{
				ATNetOutPort* bcast_outport = bcast_node->get_outports().at(j);

				// Inport that's fed as fanout of src_outport
				ATNetInPort* dest_inport = src_fanout[j];
				ATNetInPort::FaninVec& dest_fanin = dest_inport->get_fanin();
				
				// Disconnect dest node from src node, replace with connection to bcaster
				ATNetInPort::FaninVec::iterator k = 
					std::find(dest_fanin.begin(), dest_fanin.end(),	src_outport);

				// Connect downstream node to bcaster
				*k = bcast_outport;
				bcast_outport->get_fanout().push_back(dest_inport);

				// Inform bcaster of the address associated with this output
				bcast_node->set_addr(j, j);
			}

			// Connect bcaster's input to src_node
			ATNetInPort* bcast_inport = bcast_node->get_inports().front();
			src_fanout.clear();
			src_fanout.push_back(bcast_inport);
			bcast_inport->get_fanin().push_back(src_outport);
		}
	}

	for (auto i : to_add)
	{
		m_netlist.add_node(i);
	}
}


void ATManager::xform_proto_match()
{
	ATNetlist::NodeVec to_add;

	for (ATNetNode* src_node : m_netlist.nodes())
	{
		ATNetNode::OutPortVec& src_outports = src_node->get_outports();

		for (ATNetOutPort* src_outport : src_outports)
		{
			const ATLinkProtocol& src_proto = src_outport->get_proto();
			ATNetOutPort::FanoutVec& src_fanout = src_outport->get_fanout();

			for (ATNetInPort*& dest_inport : src_fanout)
			{
				ATNetNode* dest_node = dest_inport->get_node();
				const ATLinkProtocol& dest_proto = dest_inport->get_proto();

				if (src_proto.compatible_with(dest_proto))
					continue;

				ATAdapterNode* ad_node = new ATAdapterNode(src_proto, dest_proto);
				to_add.push_back(ad_node);

				ATNetOutPort* ad_outport = ad_node->get_outports().front();
				ATNetOutPort::FanoutVec& ad_fanout = ad_outport->get_fanout();
				ATNetInPort* ad_inport = ad_node->get_inports().front();
				ATNetInPort::FaninVec& ad_fanin = ad_inport->get_fanin();

				ATNetInPort::FaninVec& dest_fanin = dest_inport->get_fanin();
				ATNetInPort::FaninVec::iterator k = 
					std::find(dest_fanin.begin(), dest_fanin.end(), src_outport);
				
				*k = ad_outport;
				ad_fanout.push_back(dest_inport);

				dest_inport = ad_inport;
				ad_fanin.push_back(src_outport);
			}
		}
	}

	for (auto i : to_add)
	{
		m_netlist.add_node(i);
	}
}


void ATManager::transform_netlist()
{
	xform_arbs();
	xform_bcasts();
	xform_proto_match();
}


void ATManager::build_system()
{
	build_netlist();
	transform_netlist();

	// Instantiate every node in the netlist
	for (ATNetNode* node : m_netlist.nodes())
	{
		node->instantiate();

		if (node->get_type() == ATNetNode::INSTANCE)
		{
			ATInstanceNode* inode = (ATInstanceNode*)node;
			const std::string& name = inode->get_instance_def()->inst_name;
			m_modules[name] = inode->get_impl();
		}
	}

	// Make connections between nodes
	for (ATNetNode* src_node : m_netlist.nodes())
	{
		ATNetNode::OutPortVec& src_outports = src_node->get_outports();

		// For each outport, connect to its fanout
		for (ATNetOutPort* src_outport : src_outports)
		{
			ati_send* send = src_outport->get_impl();

			ATNetOutPort::FanoutVec& src_fanout = src_outport->get_fanout();
			assert (src_fanout.size() == 1);

			for (ATNetInPort* dest_inport : src_fanout)
			{
				ati_recv* recv = dest_inport->get_impl();

				assert(dest_inport->get_fanin().size() == 1);
				assert(dest_inport->get_proto().compatible_with(src_outport->get_proto()));

				ati_channel* chan = new ati_channel(src_outport->get_proto());
				send->bind(*chan);
				recv->bind(*chan);
			}
		}
	}
}

