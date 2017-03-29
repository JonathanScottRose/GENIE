#include "pch.h"
#include "network.h"
#include "genie_priv.h"
#include "genie/port.h"
#include "genie/genie.h"

using namespace genie::impl;
using Dir = genie::Port::Dir;

//
// Endpoint
//

Endpoint::Endpoint(NetType type, Dir dir, HierObject* parent)
	: m_dir(dir), m_type(type), m_obj(parent)
{
	const Network* def = genie::impl::get_network(type);
	m_max_links = dir == Dir::IN ?
		def->get_default_max_in_conns() :
		def->get_default_max_out_conns();
}

Endpoint::Endpoint(const Endpoint& o)
	: m_dir(o.m_dir), m_obj(nullptr), m_type(o.m_type), m_max_links(o.m_max_links)
{
}

Endpoint::~Endpoint()
{
	// Connections are owned by someone else. Don't cleanup.
}

const Network* Endpoint::get_network() const
{
	// Helper method
	return genie::impl::get_network(m_type);
}

void Endpoint::add_link(Link* link)
{
	// Already bound to this specific link
	if (has_link(link))
		throw Exception(m_obj->get_hier_path() + ": link is already bound to endpoint");

	auto n_cur_links = m_links.size();
	if (m_max_links != UNLIMITED && n_cur_links >= m_max_links)
	{
		std::string dir = m_dir == Dir::OUT ? "source" : "sink";
		throw Exception(m_obj->get_hier_path() + ": " + dir + " endpoint is limited to " + 
			std::to_string(m_max_links)	+ " connections ");
	}

	m_links.push_back(link);
}

void Endpoint::remove_link(Link* link)
{
	assert(has_link(link));
	util::erase(m_links, link);
}

bool Endpoint::has_link(Link* link) const
{
	return util::exists(m_links, link);
}

void Endpoint::remove_all_links()
{
	m_links.clear();
}

const Endpoint::Links& Endpoint::links() const
{
	return m_links;
}

Link* Endpoint::get_link0() const
{
	// Return the first one
	return m_links.empty() ? nullptr : m_links.front();
}

Endpoint::Endpoints Endpoint::get_remotes() const
{
	Endpoints result;

	// Go through all bound links and collect the things on the other side
	for (auto& link : m_links)
	{
		Endpoint* ep = link->get_other_ep(this);
		result.push_back(ep);
	}

	return result;
}

Endpoint* Endpoint::get_remote0() const
{
	if (m_links.empty())
		return nullptr;

	return m_links.front()->get_other_ep(this);
}

bool Endpoint::is_connected() const
{
	return !m_links.empty();
}

HierObject* Endpoint::get_remote_obj0() const
{
	auto other_ep = get_remote0();
	return other_ep ? other_ep->m_obj : nullptr;
}

Endpoint::Objects Endpoint::get_remote_objs() const
{
	Objects result;

	for (auto& link : m_links)
	{
		result.push_back(link->get_other_ep(this)->m_obj);
	}

	return result;
}

Endpoint* Endpoint::get_sibling() const
{
	return get_obj()->get_endpoint(m_type, m_dir.rev());
}


//
// Link
//



Link::Link(Endpoint* src, Endpoint* sink)
{
	set_src_ep(src);
	set_sink_ep(sink);
}

Link::Link()
	: Link(nullptr, nullptr)
{
}

Link::Link(const Link& o)
	: m_src(nullptr), m_sink(nullptr)
{
	// zero out src/sink when cloning
}

Link::~Link()
{
}

HierObject* Link::get_src() const
{
	return get_src_ep()->get_obj();
}

HierObject* Link::get_sink() const
{
	return get_sink_ep()->get_obj();
}

Endpoint* Link::get_src_ep() const
{
	return m_src;
}

Endpoint* Link::get_sink_ep() const
{
	return m_sink;
}

Endpoint* Link::get_other_ep(const Endpoint* ep) const
{
	assert(ep);

	// Validate
	if (ep == m_src && ep->get_dir() == Dir::OUT)
	{
		return m_sink;
	}
	else if (ep == m_sink && ep->get_dir() == Dir::IN)
	{
		return m_src;
	}

	throw Exception("degenerate link");
	return nullptr; // not reached
}

void Link::set_src_ep(Endpoint* ep)
{
	m_src = ep;
}

void Link::set_sink_ep(Endpoint* ep)
{
	m_sink = ep;
}

NetType Link::get_type() const
{
	// For now, return the network type of one of the endpoints rather than keeping
	// our own m_type field in the link itself
	Endpoint* ep = m_src ? m_src : m_sink;
	if (!ep)
		throw Exception("link not connected to anything, can not determine network type");

	return ep->get_type();
}

Link* Link::clone() const
{
	return new Link(*this);
}

//
// ALinkContainment
//

/*

ALinkContainment::ALinkContainment()
{
}

ALinkContainment::~ALinkContainment()
{
}

void genie::ALinkContainment::add_links(const Links& others, PorC porc)
{
	for (auto& other : others)
	{
		add_link(other, porc);
	}
}

void ALinkContainment::add_link(Link* other, PorC porc)
{
	assert(other);

	// Add to our list
	this->add_link_internal(other, porc);

	// Update the other link
	auto other_containment = other->asp_get<ALinkContainment>();
	if (other_containment)
	{
		Link* thislink = this->asp_container();
		other_containment->add_link_internal(thislink, rev_rel(porc));
	}
}

Link* ALinkContainment::get_link0(PorC porc) const
{
	const Links& links = get_links(porc);
	return links.empty() ? nullptr : links.front();
}

void ALinkContainment::remove_link(Link* other, PorC porc)
{
	assert(other);

	// Tell other that we're no longer associated
	auto other_containment = other->asp_get<ALinkContainment>();
	if (other_containment)
	{
		Link* thislink = this->asp_container();
		other_containment->remove_link_internal(thislink, rev_rel(porc));
	}

	// Now disassociate ourselves from the other link
	this->remove_link_internal(other, porc);
}

void ALinkContainment::remove_link_internal(Link* other, PorC porc)
{
	util::erase(m_links[porc], other);
}

void ALinkContainment::add_link_internal(Link* other, PorC porc)
{
	Links& links = m_links[porc];

	// Only insert if not existing
	auto it = std::find(links.begin(), links.end(), other);
	if (it == links.end())
		links.insert(it, other);
}

ALinkContainment::Links ALinkContainment::get_all_links(NetType type, PorC porc) const
{
	Links result;

	// Do a recursive traversal
	std::stack<Link*> to_visit;
	to_visit.push(this->asp_container());

	while (!to_visit.empty())
	{
		Link* link = to_visit.top();
		to_visit.pop();

		// Ignore links which lack containment aspect
		auto containment = link->asp_get<ALinkContainment>();
		if (!containment)
			continue;

		// Iterate over parents (or children) of currently visited link
		for (Link* other : containment->get_links(porc))
		{
			// Matches type - add to final return list
			if (other->get_type() == type)
				result.push_back(other);

			// Match or no match, add it to the stack for traversal
			to_visit.push(other);
		}
	}

	return result;
}
*/



EndpointPair::EndpointPair()
	: in(nullptr), out(nullptr)
{
}


