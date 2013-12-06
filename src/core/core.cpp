#include <fstream>
#include "common.h"
#include "core.h"
#include "export_node.h"

using namespace ct;
using namespace Core;


//
// Protocol
//

Protocol::Protocol()
{
}

Protocol::Protocol(const Protocol& o)
{
	*this = o;
}

Protocol& Protocol::operator= (const Protocol& o)
{
	for (auto& f : m_fields)
	{
		delete f;
	}

	for (auto& f : o.fields())
	{
		Field* nf = new Field(*f);
		m_fields.push_front(nf);
	}
	m_fields.reverse();

	return *this;
}

Protocol::~Protocol()
{
	Util::delete_all(m_fields);
}

void Protocol::add_field(Field* f)
{
	assert (!has_field(f->name));
	m_fields.push_front(f);
}
 
Field* Protocol::get_field(const std::string& name) const
{
	auto it = std::find_if(m_fields.begin(), m_fields.end(), [&](Field* f)
	{
		return f->name == name;
	});

	return (it == m_fields.end()) ? nullptr : *it;
}

bool Protocol::has_field(const std::string& name) const
{
	return get_field(name) != nullptr;
}

void Protocol::remove_field(Field* f)
{
	auto it = std::find(m_fields.begin(), m_fields.end(), f);
	if (it != m_fields.end())
	{
		m_fields.erase(it);
	}
}

void Protocol::delete_field(const std::string& name)
{
	auto it = std::find_if(m_fields.begin(), m_fields.end(), [&](Field* f)
	{
		return f->name == name;
	});

	if (it != m_fields.end())
	{
		delete *it;
		m_fields.erase(it);
	}
}

//
// Field
//

Field::Field()
{
}

Field::Field(const std::string& name_, int width_, Sense sense_)
: name(name_), width(width_), sense(sense_)
{
}

//
// Node
//

Node::Node()
: m_parent(nullptr)
{
}

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
	case MIXED: return MIXED;
	default: assert(false); return IN;
	}
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

	m_proto.add_field(new Field(fname, 1, Field::FWD));
}

ClockResetPort::~ClockResetPort()
{
}

//
// ConduitPort
//

ConduitPort::ConduitPort(Node* node)
	: Port(CONDUIT, MIXED, node)
{
};

ConduitPort::~ConduitPort()
{
}

//
// DataPort
//

DataPort::DataPort(Node* node, Dir dir)
	: Port(DATA, dir, node)
{
}

DataPort::~DataPort()
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
	set_sink0(sink);
}

void Conn::set_sink0(Port* sink)
{
	m_sinks.clear();
	m_sinks.push_front(sink);
}

Port* Conn::get_sink0()
{
	assert(m_sinks.size() <= 1);
	return m_sinks.empty()? nullptr : m_sinks.front();
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
// System
//

System::System(const std::string& name)
: m_name(name)
{
	m_export = new ExportNode(this);
	add_node(m_export);
}

System::~System()
{
	Util::delete_all(m_conns);
	Util::delete_all_2(m_nodes);
}

void System::connect_ports(Port* src, Port* dest)
{
	Conn* conn = new Conn(src, dest);
	src->set_conn(conn);
	dest->set_conn(conn);
	add_conn(conn);
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
	std::ofstream out("graph.dot");

	out << "digraph netlist {" << std::endl;

	for (auto& conn : m_conns)
	{
		Node* src_node = conn->get_source()->get_parent();
		Node* dest_node = conn->get_sink0()->get_parent();

		DataPort* src = (DataPort*)conn->get_source();
		if (src->get_type() != Port::DATA)
			continue;	

		out << src_node->get_name() << " -> " << dest_node->get_name()
			<< " ;" << std::endl;
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

ExportNode* System::get_export_node()
{
	return m_export;
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
	assert(node->get_parent() == nullptr || node->get_parent() == this);
	m_nodes[node->get_name()] = node;
	node->set_parent(this);
}

void System::remove_node(Node* node)
{
	assert(node != m_export);
	m_nodes.erase(node->get_name());
}

bool System::has_node(const std::string& name)
{
	return m_nodes.count(name) > 0;
}

//
// Registry
//

Registry::Registry()
{
	// hardcode here for now
	register_nodetype("export");
	register_nodetype("system");
}

Registry::~Registry()
{
	Util::delete_all(m_node_types);
}

Node::Type Registry::register_nodetype(const std::string& name)
{
	m_node_types.push_back(new NodeTypeInfo{name});
	return m_node_types.size() - 1;
}

Node::Type Registry::get_nodetype(const std::string& name)
{
	auto it = std::find_if(m_node_types.begin(), m_node_types.end(),
		[=](NodeTypeInfo* x) { return x->name == name; });

	if (it == m_node_types.end())
		throw Exception("Node type " + name + " not found");

	return it - m_node_types.begin();
}

const Registry::NodeTypeInfo* Registry::get_nodetype_info(Node::Type type)
{
	if (type >= m_node_types.size()) return nullptr;
	else return m_node_types[type];
}

const Registry::NodeTypeInfo* Registry::get_nodetype_info(const std::string& name)
{
	auto it = std::find_if(m_node_types.begin(), m_node_types.end(), 
		[=](NodeTypeInfo* x) { return x->name == name; });

	return it == m_node_types.end() ? nullptr : *it;
}

const std::string& Registry::get_nodetype_name(Node::Type type)
{
	assert(type < m_node_types.size());
	return m_node_types[type]->name;
}
