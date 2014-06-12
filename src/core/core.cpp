#include <fstream>
#include "ct/p2p.h"
#include "ct/common.h"
#include "ct/spec.h"
#include "ct/export_node.h"
#include "ct/instance_node.h" // bad

using namespace ct;
using namespace P2P;


//
// Node
//

Node::Node(Type type)
	: m_type(type), m_parent(nullptr)
{
}

Node::~Node()
{
	Util::delete_all_2(m_ports);
}

void Node::add_port(Port* port)
{
	assert(!port->get_name().empty());
	assert(!Util::exists_2(m_ports, port->get_name()));
	m_ports.emplace(port->get_name(), port);
}

void Node::remove_port(Port* port)
{
	m_ports.erase(port->get_name());
}

void Node::remove_port(const std::string& name)
{
	m_ports.erase(name);
}

//
// Port
//

Port::Port(Type type, Dir dir, Node* node)
	: m_parent(node), m_type(type), m_conn(nullptr), m_dir(dir)
{
}


Port::~Port()
{
}

Port::Dir Port::rev_dir(Dir dir)
{
	switch (dir)
	{
	case IN : return OUT;
	case OUT: return IN;
	default: assert(false); return IN;
	}
}

void Port::clear_flows()
{
	m_flows.clear();
}


void Port::add_flow(Flow* flow)
{
	m_flows.push_back(flow);
}


void Port::add_flows(const Flows& f)
{
	m_flows.insert(m_flows.end(), f.begin(), f.end());
}

bool Port::has_flow(Flow* flow)
{
	return std::find(m_flows.begin(), m_flows.end(), flow) != m_flows.end();
}

Port* Port::get_driver()
{
	switch (m_dir)
	{
		case OUT: return this; break;
		case IN: return m_conn? m_conn->get_source() : nullptr; break;
	}

	return nullptr;
}

Port* Port::get_first_connected_port()
{
	if (!m_conn)
		return nullptr;

	Port* result = nullptr;
	switch (m_dir)
	{
		case IN: result =  m_conn->get_source(); break;
		case OUT: result =  m_conn->get_sink(); break;
	}

	assert(result != this);
	return result;
}

bool Port::is_connected()
{
	return get_conn() != nullptr;
}


//
// ClockResetPort
//

ClockResetPort::ClockResetPort(Type type, Dir dir, Node* node)
	: Port(type, dir, node)
{
	std::string fname;
	switch (type)
	{
	case Port::CLOCK: fname = "clock"; break;
	case Port::RESET: fname = "reset"; break;
	default: assert(false);
	}

	m_proto.init_field(fname, 1);
}

//
// ConduitPort
//

ConduitPort::ConduitPort(Node* node, Dir dir)
	: Port(CONDUIT, dir, node)
{
};

//
// DataPort
//

DataPort::DataPort(Node* node, Dir dir)
	: Port(DATA, dir, node), m_clock(nullptr)
{
}

//
// Connection
//

Conn::Conn()
	: m_source(nullptr)
{
}

Conn::Conn(Port* src, Port* sink)
	: m_source(src)
{
	set_sink(sink);
}

void Conn::set_sink(Port* sink)
{
	m_sinks.clear();
	m_sinks.push_front(sink);
}

Port* Conn::get_sink()
{
	return m_sinks.front();
}

void Conn::add_sink(Port* sink)
{
	m_sinks.push_front(sink);
}

void Conn::remove_sink(Port* sink)
{
	m_sinks.remove(sink);
}

//
// FlowTarget
//

FlowTarget::FlowTarget()
	: port(nullptr)
{
}

FlowTarget::FlowTarget(Port* _port, const Spec::LinkTarget& _lt)
	: port(_port), lt(_lt)
{
}

Spec::Linkpoint* FlowTarget::get_linkpoint() const
{
	auto inst_node = (InstanceNode*) port->get_parent();
	assert (inst_node->get_type() == Node::INSTANCE);
	Spec::Instance* inst_def = inst_node->get_instance();
	Spec::System* sys_spec = inst_def->get_parent();
	return sys_spec->get_linkpoint(lt);
}

bool FlowTarget::operator== (const FlowTarget& other) const
{
	return port == other.port && lt == other.lt;
}

//
// Flow
//

Flow::Flow()
{
	m_src = new FlowTarget();
}

Flow::~Flow()
{
	delete m_src;
	Util::delete_all(m_sinks);
}

FlowTarget* Flow::get_sink()
{
	return m_sinks.front();
}

void Flow::set_sink(Port* port, const Spec::LinkTarget& lt)
{
	m_sinks.clear();
	m_sinks.push_front(new FlowTarget(port, lt));
}

void Flow::add_sink(Port* port, const Spec::LinkTarget& lt)
{
	m_sinks.push_front(new FlowTarget(port, lt));
}

void Flow::remove_sink(FlowTarget* sink)
{
	m_sinks.remove(sink);
}

bool Flow::has_sink(Port* port)
{
	return std::find_if(m_sinks.begin(), m_sinks.end(), [=] (FlowTarget* t)
	{
		return t->port == port;
	}) != m_sinks.end();
}

void Flow::set_src(Port* port, const Spec::LinkTarget& lt)
{
	m_src->port = port;
	m_src->lt = lt;
}

FlowTarget* Flow::get_src()
{
	return m_src;
}

FlowTarget* Flow::get_sink(Port* port)
{
	for (auto& i : m_sinks)
	{
		if (i->port == port)
			return i;
	}

	assert(false);
	return nullptr;
}

FlowTarget* Flow::get_sink0()
{
	if (m_sinks.empty())
		return nullptr;

	return m_sinks.front();
}

//
// System
//

System::System(const std::string& name)
: m_name(name)
{
}

System::~System()
{
	Util::delete_all(m_conns);
	Util::delete_all_2(m_flows);
}

void System::add_flow(Flow* flow)
{
	m_flows[flow->get_id()] = flow;
}

Conn* System::connect_ports(Port* src, Port* dest)
{
	Conn* conn = src->get_conn();
	if (!conn)
	{
		conn = new Conn();
		conn->set_source(src);
		src->set_conn(conn);
		add_conn(conn);
	}

	conn->add_sink(dest);
	dest->set_conn(conn);
	return conn;
}

void System::disconnect_ports(Port* src, Port* dest)
{
	Conn* conn = src->get_conn();
	assert(conn == dest->get_conn());

	src->set_conn(nullptr);
	dest->set_conn(nullptr);

	remove_conn(conn);
}

void System::remove_conn(Conn* conn)
{
	m_conns.erase(std::find(m_conns.begin(), m_conns.end(), conn));
	delete conn;
}

void System::dump_graph()
{
	std::ofstream out(get_name() + ".dot");

	out << "digraph netlist {" << std::endl;

	for (auto& conn : m_conns)
	{
		Port* src = conn->get_source();
		Node* src_node = src->get_parent();

		if (src->get_type() == Port::CLOCK ||
			src->get_type() == Port::RESET)
			continue;

		for (auto& dest : conn->get_sinks())
		{
			Node* dest_node = dest->get_parent();
		
			out << src_node->get_name() << " -> " << dest_node->get_name()
				<< " [taillabel=\"" << src->get_name() << "\"]" 
				<< " [headlabel=\"" << dest->get_name() << "\"];"
				<< std::endl;
		}
	}

	out << "}" << std::endl;
}

void System::add_conn(Conn* conn)
{
	m_conns.push_back(conn);
}

ClockResetPort* System::get_a_reset_port()
{
	ExportNode* exp = get_export_node();

	for (auto& port : exp->ports())
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

void System::connect_clock_src(DataPort* target_port, ClockResetPort* clock_src)
{
	auto target_clock_port = target_port->get_clock();

	// hack: if the target port references a null clock port, the target port probably
	// belongs to an export node. in this case, the clock source is likely ON the export node
	// itself, so let's set it here.
	if (!target_clock_port)
	{
		assert(clock_src->get_parent()->get_type() == Node::EXPORT &&
			clock_src->get_parent() == target_port->get_parent());

		target_port->set_clock(clock_src);
		return; // done here, no connections need to be made
	}
	
	auto existing_clock_src = target_clock_port->get_driver();
	if (existing_clock_src)
	{
		// we don't know how to handle this otherwise yet
		assert(existing_clock_src == clock_src);
	}
	else
	{
		connect_ports(clock_src, target_clock_port);
	}
}

ExportNode* System::get_export_node()
{
	const std::string& exname = get_name();
	ExportNode* result = (ExportNode*)get_node(exname);

	if (!result)
	{
		result = new ExportNode(this);
		add_node(result);
	}

	assert(result->get_type() == Node::EXPORT);
	return result;
}

Node* System::get_node(const std::string& name)
{
	if (!has_node(name))
		return nullptr;

	return m_nodes[name];
}

void System::add_node(Node* node)
{
	assert(!has_node(node->get_name()));
	assert(node->get_parent() == nullptr);
	m_nodes[node->get_name()] = node;
	node->set_parent(this);
}

bool System::has_node(const std::string& name)
{
	return m_nodes.count(name) > 0;
}

int System::get_global_flow_id_width()
{
	int result = 0;
	for (auto& i: m_flows)
	{
		result = std::max(result, i.first);
	}
	result = Util::log2(result);
	result = std::max(result, 1);	// handle corner case: one flow with id 0
	return result;
}

void System::splice_conn(Conn* conn, Port* new_in, Port* new_out)
{
	Port* orig_out = conn->get_source();
	Port* orig_in = conn->get_sink();

	disconnect_ports(orig_out, orig_in);
	connect_ports(orig_out, new_in);
	connect_ports(new_out, orig_in);
	
	// copy flows
	auto flows = orig_in->flows();
	new_in->add_flows(flows);
	new_out->add_flows(flows);
}