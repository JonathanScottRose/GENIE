#include "pch.h"
#include "network.h"
#include "node.h"
#include "node_system.h"
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
	const NetworkDef* def = genie::impl::get_network(type);
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

const NetworkDef* Endpoint::get_network() const
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
// LinkRelations
//

void LinkRelations::add(Link * parent, Link * child)
{
	using namespace graph;

	// Get or create vertices for links
	VertexID par_v;
	VertexID child_v;

	auto par_it = m_link2v.find(parent);
	auto child_it = m_link2v.find(child);

	if (par_it == m_link2v.end())
	{
		par_v = m_graph.newv();
		m_link2v[parent] = par_v;
		m_v2link[par_v] = parent;
	}
	else
	{
		par_v = par_it->second;
	}

	if (child_it == m_link2v.end())
	{
		child_v = m_graph.newv();
		m_link2v[child] = child_v;
		m_v2link[child_v] = child;
	}
	else
	{
		child_v = child_it->second;
	}

	m_graph.newe(par_v, child_v);
}

void LinkRelations::remove(Link * parent, Link * child)
{
	using namespace graph;

	auto par_it = m_link2v.find(parent);
	if (par_it == m_link2v.end())
		return;

	auto child_it = m_link2v.find(child);
	if (child_it == m_link2v.end())
		return;

	VertexID par_v = par_it->second;
	VertexID child_v = par_it->second;

	EdgeID e = m_graph.dir_edge(par_v, child_v);
	if (e != INVALID_E)
		m_graph.dele(e);
}

bool LinkRelations::is_contained_in(Link * parent, Link * child) const
{
	using namespace graph;

	auto par_it = m_link2v.find(parent);
	assert(par_it != m_link2v.end());
	
	auto child_it = m_link2v.find(child);
	assert(child_it != m_link2v.end());

	VertexID par_v = par_it->second;
	VertexID child_v = par_it->second;

	EdgeID e = m_graph.dir_edge(par_v, child_v);
	return e != INVALID_E;
}

void LinkRelations::unregister_link(Link * link)
{
	using namespace graph;

	auto it = m_link2v.find(link);
	assert(it != m_link2v.end());

	VertexID vid = it->second;
	m_link2v.erase(it);
	m_v2link.erase(vid);

	m_graph.delv(vid);
}

void LinkRelations::get_porc_internal(Link * link, NetType net, 
	graph::VList(graph::Graph::* grafunc)(graph::VertexID) const, void * pout,
	const ThuncFunc& thunk) const
{
	using namespace graph;
	std::vector<void*>& out = *((std::vector<void*>*)pout);
	
	auto it = m_link2v.find(link);
	assert(it != m_link2v.end());

	VertexID firstv = it->second;
	
	std::stack<VertexID> to_visit;
	to_visit.push(firstv);

	while (!to_visit.empty())
	{
		VertexID curv = to_visit.top();
		to_visit.pop();

		// Expand.
		// Grafunc either points to the dir_neigh or dir_neigh_r member function, controlling
		// which direction the graph expansion goes.
		VList children = (m_graph.*grafunc)(curv);
		for (auto& v : children) to_visit.push(v);

		// Don't process the first vertex
		if (curv == firstv)
			continue;

		// Get link corresponding to vertex, return it if it's the right type
		Link* link = m_v2link.find(curv)->second;
		if (link->get_type() == net)
		{
			// Cast the link pointer to a derived type, and do pointer adjustment, using
			// the thunk function
			out.push_back(thunk(link));
		}
	}
}

LinkRelations* LinkRelations::clone(const Node * srcsys, Node * destsys) const
{
	// Create empty
	LinkRelations* result = new LinkRelations;

	// Copy the graph verbatim
	result->m_graph = m_graph;

	// Traverse links(= vertices) in source (this)
	for (auto& it : m_v2link)
	{
		auto v = it.first;
		auto& link = it.second;

		auto srcname = link->get_src()->get_hier_path(srcsys);
		auto sinkname = link->get_sink()->get_hier_path(srcsys);

		// Get version of old link in new system, using endpoint names.
		// It's ok if it doesn't exist - just ignore the link
		auto newsrc = dynamic_cast<HierObject*>(destsys->get_child(srcname));
		auto newsink = dynamic_cast<HierObject*>(destsys->get_child(sinkname));
		if (!newsrc || !newsink)
		{
			result->m_graph.delv(v);
			continue;
		}

		// Get the link in new system, if it exists
		auto newlinks = destsys->get_links(newsrc, newsink, link->get_type());
		if (newlinks.empty())
		{
			result->m_graph.delv(v);
			continue;
		}

		// Might want to handle this properly later
		assert(newlinks.size() == 1);

		// Insert v<->edge mapping into new relations structure.
		// The vertex exists.
		result->m_v2link[v] = newlinks.front();
		result->m_link2v[link] = v;
	}

	// (The edges were copied from the source graph. Some may have been pruned)

	return result;
}

void LinkRelations::reintegrate(LinkRelations * src, const Node* srcsys,
	const Node* destsys)
{
	// Perform a union of the graphs
	m_graph.union_with(src->m_graph);
	
	// Copy only new entries from the v<->link maps
	for (auto src_v2link : src->m_v2link)
	{
		auto v = src_v2link.first;
		auto src_link = src_v2link.second;

		if (m_v2link.count(v) == 0)
		{
			// The link pointer needs to be updated.
			// The link can exist in the src system, or in the dest system (it's been moved).
			// Need to try both possibilities before giving up.
			std::string srcname;
			std::string sinkname;

			if (srcsys->is_parent_of(src_link->get_src()))
				srcname = src_link->get_src()->get_hier_path(srcsys);
			else if (destsys->is_parent_of(src_link->get_src()))
				srcname = src_link->get_src()->get_hier_path(destsys);
			else
				assert(false);

			if (srcsys->is_parent_of(src_link->get_sink()))
				sinkname = src_link->get_sink()->get_hier_path(srcsys);
			else if (destsys->is_parent_of(src_link->get_sink()))
				sinkname = src_link->get_sink()->get_hier_path(destsys);

			auto newsrc = dynamic_cast<HierObject*>(destsys->get_child(srcname));
			auto newsink = dynamic_cast<HierObject*>(destsys->get_child(sinkname));
			assert(newsrc && newsink);
			
			auto newlinks = destsys->get_links(newsrc, newsink, src_link->get_type());
			assert(newlinks.size() == 1);

			auto link = newlinks.front();

			// New entry
			m_v2link[v] = link;
			m_link2v[link] = v;
		}
	}
}


EndpointPair::EndpointPair()
	: in(nullptr), out(nullptr)
{
}


