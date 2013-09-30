#include "system_node.h"

using namespace ct::P2P;

SystemNode::SystemNode()
	: Node(SYSTEM)
{
}


SystemNode::~SystemNode()
{
	Util::delete_all_2(m_nodes);
	Util::delete_all(m_conns);
	Util::delete_all_2(m_flows);
}


void SystemNode::add_node(Node *node)
{
	assert(!node->get_name().empty());
	assert(m_nodes.find(node->get_name()) == m_nodes.end());

	if (node->get_parent() == nullptr)
	{
		node->set_parent(this);
	}
	else
	{
		assert(node->get_parent() == this);
	}

	m_nodes.emplace(node->get_name(), node);
}


void SystemNode::remove_node(Node *node)
{
	m_nodes.erase(node->get_name());
}


void SystemNode::add_flow(Flow* flow)
{
	m_flows[flow->get_id()] = flow;
}

void SystemNode::connect_ports(DataPort* src, DataPort* dest)
{
	Conn* conn = new Conn(src, dest);
	src->set_conn(conn);
	dest->set_conn(conn);
	add_conn(conn);
}

void SystemNode::disconnect_ports(DataPort* src, DataPort* dest)
{
	Conn* conn = src->get_conn();
	assert(conn == dest->get_conn());

	src->set_conn(nullptr);
	dest->set_conn(nullptr);
	
	remove_conn(conn);
}

void SystemNode::remove_conn(Conn* conn)
{
	m_conns.erase(std::find(m_conns.begin(), m_conns.end(), conn));
	delete conn;
}

void SystemNode::dump_graph()
{
	std::ofstream out("graph.dot");

	out << "digraph netlist {" << std::endl;

	for (auto& conn : m_conns)
	{
		Node* src_node = conn->get_source()->get_parent();
		Node* dest_node = conn->get_sink()->get_parent();

		DataPort* src = (DataPort*)conn->get_source();
		if (src->get_type() != Port::DATA)
			continue;

		std::string label;
		for (Flow* flow : src->flows())
		{
			label += ' ' + std::to_string(flow->get_id());
		}

		out << src_node->get_name() << " -> " << dest_node->get_name() 
			<< " [label=\"" << label << "\"];" << std::endl;
	}

	out << "}" << std::endl;
}

void SystemNode::add_conn(Conn* conn)
{
	m_conns.push_back(conn);
}

ClockResetPort* SystemNode::get_a_reset_port()
{
	for (auto& port : ports())
	{
		ClockResetPort* p = (ClockResetPort*)port.second;
		if (p->get_type() == Port::RESET &&
			p->get_dir() == Port::OUT)
		{
			return p;
		}
	}
	
	assert(false);
	return nullptr;
}

