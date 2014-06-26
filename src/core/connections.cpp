#include "ct/ct.h"
#include "ct/connections.h"

using namespace ct;

//
// Endpoint
//

Endpoint::~Endpoint()
{
	// Connections are owned by someone else. Don't cleanup.
}

void Endpoint::ep_connect(Conn* c)
{
	NetworkDef* nd = _get_net_def();

	switch (ep_get_dir())
	{
		case Dir::OUT:
		{
			if (ep_is_connected() && !nd->allow_src_multibind())
				throw Exception("Source endpoint already connected");
			break;
		}

		case Dir::IN:
		{
			if (ep_is_connected() && !nd->allow_sink_multibind())
				throw Exception("Sink endpoint already connected");
			break;
		}
	}

	if (!Util::exists(m_conns, c))
		m_conns.push_back(c);
}

void Endpoint::ep_disconnect()
{
	m_conns.clear();
}

void Endpoint::ep_disconnect(Conn* c)
{
	assert(c);

	if (!ep_has_conn(c))
		throw Exception("Connection not found");

	Util::erase(m_conns, c);
}

const Conns& Endpoint::ep_conns() const
{
	return m_conns;
}

Conn* Endpoint::ep_conn0() const
{
	// Return the first one
	return m_conns.front();
}

Endpoints Endpoint::ep_get_remotes() const
{
	Endpoints result;
	Dir my_dir = ep_get_dir();

	// Go through all bound connections and collect the things on the other side
	for (auto& c : m_conns)
	{
		switch (my_dir)
		{
			case Dir::OUT:
			{
				auto fanout = c->get_sinks();
				result.insert(result.end(), fanout.begin(), fanout.end());
				break;
			}

			case Dir::IN:
			{
				result.push_back(c->get_src());
				break;
			}
		}
	}

	return result;
}

Endpoint* Endpoint::ep_get_remote0() const
{
	if (!ep_is_connected())
		return nullptr;

	return ep_get_remotes().front();
}

bool Endpoint::ep_has_conn(Conn* c) const
{
	return Util::exists(m_conns, c);
}

bool Endpoint::ep_is_connected() const
{
	return !m_conns.empty();
}

Dir Endpoint::ep_get_dir() const
{
	return m_parent->conn_get_dir();
}

NetworkDef* Endpoint::_get_net_def() const
{
	return ct::get_root()->get_net_def(ep_get_net_type());
}

//
// Conn
//

Conn::Conn(const NetworkID& net)
: Conn(net, nullptr, nullptr)
{
}

Conn::Conn(const NetworkID& net, Endpoint* src, Endpoint* sink)
: m_net_type(net), m_src(src)
{
	if (sink) m_sinks.push_back(sink);
}

Endpoint* Conn::get_src() const
{
	return m_src;
}

Endpoint* Conn::get_sink0() const
{
	if (m_sinks.empty())
		return nullptr;
	else
		return m_sinks.front();
}

Endpoints Conn::get_sinks() const
{
	return m_sinks;
}

const Endpoints& Conn::sinks() const
{
	return m_sinks;
}

void Conn::set_src(Endpoint* ep)
{
	if (ep && m_src)
		throw Exception("Conn already has src");

	m_src = ep;
}

void Conn::add_sink(Endpoint* ep)
{
	NetworkDef* nd = ct::get_root()->get_net_def(m_net_type);
	if (!m_sinks.empty() && !nd->allow_conn_multibind())
		throw Exception("Conn already has sink");

	if (!has_sink(ep))
		m_sinks.push_back(ep);
}

void Conn::remove_sink(Endpoint* ep)
{
	Util::erase(m_sinks, ep);
}

bool Conn::has_sink(Endpoint* ep) const
{
	return Util::exists(m_sinks, ep);
}

