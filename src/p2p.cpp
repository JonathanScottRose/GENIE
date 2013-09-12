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
	for (auto& f : o.fields())
	{
		Field* nf = new Field;
		nf->name = f->name;
		nf->width = f->width;
		nf->sense = f->sense;
		
		m_fields.push_front(nf);
	}
}

Protocol& Protocol::operator= (const Protocol& o)
{
	for (auto& f : m_fields)
	{
		delete f;
	}

	for (auto& f : o.fields())
	{
		Field* nf = new Field;
		nf->name = f->name;
		nf->width = f->width;
		nf->sense = f->sense;
		
		m_fields.push_front(nf);
	}

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

bool Protocol::has_field(const std::string& name)
{
	return get_field(name) != nullptr;
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
// FieldBinding
//

FieldBinding::FieldBinding()
	: m_sig_def(nullptr)
{
}

FieldBinding::FieldBinding(const std::string& name, Spec::Signal* sig_def)
	: m_name(name), m_sig_def(sig_def)
{
}

FieldBinding::~FieldBinding()
{
	if (m_sig_def) delete m_sig_def;
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

ClockResetPort::ClockResetPort(Type type, Dir dir, Node* node, Spec::Signal* sigdef)
	: Port(type, dir, node), m_sigdef(sigdef)
{
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


void DataPort::add_flows(const FlowVec& f)
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
// Flow
//

DataPort* Flow::get_sink()
{
	return m_sinks.front();
}

void Flow::set_sink(DataPort* sink)
{
	m_sinks.clear();
	m_sinks.push_front(sink);
}

void Flow::add_sink(DataPort* sink)
{
	m_sinks.push_front(sink);
}

void Flow::remove_sink(DataPort* sink)
{
	m_sinks.remove(sink);
}

bool Flow::has_sink(DataPort* sink)
{
	return std::find(m_sinks.begin(), m_sinks.end(), sink) != m_sinks.end();
}
