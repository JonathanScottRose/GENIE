#include <algorithm>
#include "at_netlist.h"
#include "at_util.h"
#include "at_instance_node.h"
#include "at_spec.h"


ATNetlist::ATNetlist()
{
}


ATNetlist::~ATNetlist()
{
	at_delete_all(m_nodes);
}


void ATNetlist::add_node(ATNetNode *node)
{
	m_nodes.push_back(node);
	if (node->get_type() == ATNetNode::INSTANCE)
	{
		ATInstanceNode* inode = (ATInstanceNode*)node;
		ATInstanceDef* idef = inode->get_instance_def();
		m_inst_node_map[idef->inst_name] = inode;
	}
}


void ATNetlist::remove_node(ATNetNode *node)
{
	std::remove(m_nodes.begin(), m_nodes.end(), node);
	if (node->get_type() == ATNetNode::INSTANCE)
	{
		ATInstanceNode* inode = (ATInstanceNode*)node;
		ATInstanceDef* idef = inode->get_instance_def();
		m_inst_node_map.erase(idef->inst_name);
	}
}


ATInstanceNode* ATNetlist::get_instance_node(const std::string& name)
{
	return m_inst_node_map[name];
}


void ATNetlist::clear()
{
	m_nodes.clear();
}


ATNetNode::ATNetNode()
{
}


ATNetNode::~ATNetNode()
{
	at_delete_all(m_inports);
	at_delete_all(m_outports);
}


ATNetInPort::ATNetInPort(ATNetNode* node)
: m_node(node)
{
}


ATNetOutPort::ATNetOutPort(ATNetNode* node)
: m_node(node)
{
}


