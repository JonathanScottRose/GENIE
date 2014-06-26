#include "ct/structure.h"

using namespace ct;

//
// NodeDef
//

const std::string NodeDef::ID = "nodedef";

NodeDef::NodeDef()
{
}

NodeDef::~NodeDef()
{
	Util::delete_all_2(m_port_defs);
}

const std::string& NodeDef::hier_name() const
{
	return m_name;
}

HierNode* NodeDef::hier_parent() const
{
	return nullptr;
}

HierNode* NodeDef::hier_child(const std::string& name) const
{
	return get_port_def(name);
}

HierNode::Children NodeDef::hier_children() const
{
	return Util::values<Children>(m_port_defs);
}

Node* NodeDef::instantiate()
{
	 Node* result = new Node();
	 result->set_node_def(this);

	 for (auto& it : m_port_defs)
	 {
		 PortDef* port_def = it.second;
		 result->add_port(port_def->instantiate());
	 }

	 return result;
}

//
// PortDef
//

PortDef::PortDef()
{
}

PortDef::~PortDef()
{
}

const std::string& PortDef::hier_name() const
{
	return m_name;
}

HierNode* PortDef::hier_parent() const
{
	return m_parent;
}

//
// Node
//

Node::Node()
{
}

Node::~Node()
{
	Util::delete_all_2(m_ports);
}

const std::string& Node::hier_name() const
{
	return m_name;
}

HierNode* Node::hier_parent() const
{
	return m_parent;
}

HierNode::Children Node::hier_children() const
{
	return Util::values<HierNode::Children>(m_ports);
}

//
// Port
//

Port::Port()
{
}

Port::~Port()
{
}

const std::string& Port::hier_name() const
{
	return m_name;
}

HierNode* Port::hier_parent() const
{
	return m_parent;
}

//
// System
//

const std::string System::ID = "system";

System::System()
{
}

System::~System()
{
	Util::delete_all_2(m_nodes);
	Util::delete_all(m_conns);
}

const std::string& System::hier_name() const
{
	return m_name;
}

HierNode* System::hier_parent() const
{
	return nullptr;
}

HierNode* System::hier_child(const std::string& name) const
{
	return get_node(name);
}

HierNode::Children System::hier_children() const
{
	return Util::values<HierNode::Children>(m_nodes);
}

Conns System::get_conns(const NetworkID& net) const
{
	Conns result;
	std::copy_if(m_conns.begin(), m_conns.end(), result.begin(), [=](Conn* i)
	{
		return i->get_net_type() == net;
	});
	return result;
}

Conn* System::connect(Endpoint* src, Endpoint* sink)
{
	assert(src && sink);
	return connect(src, Endpoints(1, sink));
}

Conn* System::connect(Endpoint* src, const Endpoints& sinks)
{
	assert(src && !sinks.empty());

	const NetworkID& net = src->ep_get_net_type();
	
	// Create new connection with src as the source
	Conn* c = new Conn(net);
	c->set_src(src);

	// Register it
	m_conns.push_back(c);

	// Connect the source to the new connection
	src->ep_connect(c);
	
	// Connect the sinks
	for (const auto& i : sinks)
	{
		const NetworkID& net_d = i->ep_get_net_type();
		if (net != net_d)
			throw Exception("Incompatible endpoints: " + net + " and " + net_d);

		i->ep_connect(c);
	}

	return c;
}

void System::disconnect(Conn* c)
{
	assert(c);
	Endpoint* src = c->get_src();

	// Remove connection from source
	assert(src);
	src->ep_disconnect(c);

	// Remove connection from all sinks
	for (auto& i : c->sinks())
	{
		i->ep_disconnect(c);
	}

	// De-register and free the connection
	Util::erase(m_conns, c);
	delete c;
}

void System::disconnect(Endpoint* ep)
{
	assert(ep);

	Conns conns = ep->ep_conns();

	switch(ep->ep_get_dir())
	{
		// Endpoint is source: kill all outgoing connections
		case Dir::OUT:
		{
			for (auto& i : conns)
			{
				disconnect(i);
			}
			break;
		}

		// Endpoint is sink: remove self from all incoming connections,
		// and delete each connection if it is now empty as a result
		case Dir::IN:
		{
			for (auto& i : conns)
			{
				// Remove this sink, and delete the connection if no more sinks
				i->remove_sink(ep);
				if (i->sinks().empty())
					disconnect(i);
			}
			break;
		}
	}	
}

void System::disconnect(Endpoint* src, Endpoint* sink)
{
	assert(src && sink);

	if (src->ep_get_dir() != Dir::OUT ||
		sink->ep_get_dir() != Dir::IN)
	{
		throw Exception("disconnect: bad directions on src/sink");
	}

	// For all outgoing connections, remove the sink from each connection.
	// If the result leaves the connection with no more sinks, kill it
	Conns conns = src->ep_conns();
	for (auto& i : conns)
	{
		i->remove_sink(sink);
		if (i->sinks().empty())
			disconnect(i);
	}
}