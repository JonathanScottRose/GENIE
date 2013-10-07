#include <fstream>
#include "p2p.h"
#include "common.h"
#include "spec.h"

using namespace ct;
using namespace P2P;


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
	: name(name_), width(width_), sense(sense_), is_const(false)
{
}

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
// DataPort
//

DataPort::DataPort(Node* node, Dir dir)
	: Port(DATA, dir, node)
{
}

DataPort::~DataPort()
{
}

void DataPort::clear_flows()
{
	m_flows.clear();
}


void DataPort::add_flow(Flow* flow)
{
	m_flows.push_back(flow);
}


void DataPort::add_flows(const Flows& f)
{
	m_flows.insert(m_flows.end(), f.begin(), f.end());
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

FlowTarget::FlowTarget(DataPort* _port, const Spec::LinkTarget& _lt)
	: port(_port), lt(_lt)
{
}

Spec::Linkpoint* FlowTarget::get_linkpoint() const
{
	return Spec::get_linkpoint(lt);
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

void Flow::set_sink(DataPort* port, const Spec::LinkTarget& lt)
{
	m_sinks.clear();
	m_sinks.push_front(new FlowTarget(port, lt));
}

void Flow::add_sink(DataPort* port, const Spec::LinkTarget& lt)
{
	m_sinks.push_front(new FlowTarget(port, lt));
}

void Flow::remove_sink(FlowTarget* sink)
{
	m_sinks.remove(sink);
}

bool Flow::has_sink(DataPort* port)
{
	return std::find_if(m_sinks.begin(), m_sinks.end(), [=] (FlowTarget* t)
	{
		return t->port == port;
	}) != m_sinks.end();
}

void Flow::set_src(DataPort* port, const Spec::LinkTarget& lt)
{
	m_src->port = port;
	m_src->lt = lt;
}

FlowTarget* Flow::get_src()
{
	return m_src;
}

FlowTarget* Flow::get_sink(DataPort* port)
{
	for (auto& i : m_sinks)
	{
		if (i->port == port)
			return i;
	}

	assert(false);
	return nullptr;
}
