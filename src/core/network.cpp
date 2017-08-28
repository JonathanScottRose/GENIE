#include "pch.h"
#include "network.h"
#include "node.h"
#include "genie_priv.h"
#include "genie/port.h"
#include "genie/genie.h"

using namespace genie::impl;
using Dir = genie::Port::Dir;

//
// LinkID
//

void LinkID::set_index(uint16_t idx)
{
	_id = (_id & 0xFFFF0000U) | idx;
}

uint16_t LinkID::get_index() const
{
	return (uint16_t)(_id & 0xFFFFU);
}

void LinkID::set_type(NetType type)
{
	_id = (_id & 0xFFFFU) | (type << 16U);
}

NetType LinkID::get_type() const
{
	return (NetType)(_id >> 16U);
}

bool LinkID::operator<(const LinkID & o) const
{
	return _id < o._id;
}

bool LinkID::operator==(const LinkID & o) const
{
	return _id == o._id;
}

bool LinkID::operator!=(const LinkID & o) const
{
	return _id != o._id;
}

LinkID::LinkID(uint32_t val)
	:_id(val)
{
}

LinkID::operator uint32_t() const
{
	return _id;
}

//
// LinksContainer
//

LinksContainer::LinksContainer()
	: m_type(NET_INVALID), m_next_id(0)
{
}

LinkID LinksContainer::insert_new(Link *link)
{
	auto index = m_next_id++;
	LinkID result(m_type, index);
	link->set_id(result);
	
	ensure_size_for_index(index);
	m_links[index] = link;

	return result;
}

void LinksContainer::insert_existing(Link *link)
{
	auto id = link->get_id();
	assert(id != LINK_INVALID);
	auto index = id.get_index();
	
	ensure_size_for_index(index);

	m_links[index] = link;
	m_next_id = std::max(m_next_id + 1, index + 1);
}

std::vector<Link*> LinksContainer::move_new_from(LinksContainer &o)
{
	std::vector<Link*> result;

	auto my_size = m_links.size();
	auto new_size = o.m_links.size();

	// Move new contents to us.
	if (new_size > my_size)
	{
		auto begin = o.m_links.begin() + my_size;
		auto end = o.m_links.end();

		m_links.insert(m_links.end(), begin, end);

		// Flatten nulls
		std::copy_if(begin, end, std::back_inserter(result), [](Link* l)
		{
			return l != nullptr;
		});
	}

	// Shrink other
	o.m_links.resize(my_size);

	// Update ID
	m_next_id = (uint16_t)new_size;

	return result;
}

void LinksContainer::prepare_for_copy(const LinksContainer &o)
{
	m_next_id = o.m_next_id;
	if (m_next_id >= m_links.size())
		m_links.resize(m_next_id + 1, nullptr);
}

Link * LinksContainer::get(LinkID id)
{
	auto idx = id.get_index();
	return (idx >= m_links.size()) ? nullptr : m_links[idx];
}

Link * LinksContainer::remove(LinkID id)
{
	auto idx = id.get_index();
	auto where = m_links.begin() + idx;
	auto result = *where;
	*where = nullptr; // leave gap in vector
	
	return result;
}

std::vector<Link*> LinksContainer::get_all() const
{
	std::vector<Link*> result;
	std::copy_if(m_links.begin(), m_links.end(), std::back_inserter(result),
		[](Link* l) { return l != nullptr; });
	
	return result;
}

void LinksContainer::get_all_internal(void * pout, const ThuncFunc &thunc) const
{
	auto& out = *((std::vector<void*>*)pout);

	for (auto plink : m_links)
	{
		// Ignore nullptrs
		if (plink)
		{
			out.push_back(thunc(plink));
		}
	}
}

void LinksContainer::ensure_size_for_index(uint16_t index)
{
	// expand the vector if need be, and fill
	// new entries with nulls
	unsigned cur_last = m_links.size();
	if (index >= cur_last)
	{
		m_links.resize(index + 1, nullptr);
	}
}


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
	: m_id(LINK_INVALID)
{
	set_src_ep(src);
	set_sink_ep(sink);
}

Link::Link()
	: Link(nullptr, nullptr)
{
}

Link::Link(const Link& o)
	: m_src(nullptr), m_sink(nullptr), m_id(o.m_id)
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

void Link::disconnect_src()
{
	auto ep = get_src_ep();
	ep->remove_link(this);
	set_src_ep(nullptr);
}

void Link::disconnect_sink()
{
	auto ep = get_sink_ep();
	ep->remove_link(this);
	set_sink_ep(nullptr);
}

void Link::reconnect_src(Endpoint* ep)
{
	set_src_ep(ep);
	ep->add_link(this);
}

void Link::reconnect_sink(Endpoint* ep)
{
	set_sink_ep(ep);
	ep->add_link(this);
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
	return m_id.get_type();
}

Link* Link::clone() const
{
	return new Link(*this);
}

//
// LinkRelations
//

void LinkRelations::add(LinkID parent, LinkID child)
{
	using namespace graph;

	// Get or create vertices for links
	VertexID par_v = (VertexID)parent;
	VertexID child_v = (VertexID)child;

	if (!m_graph.hasv(par_v))
		m_graph.newv(par_v);

	if (!m_graph.hasv(child_v))
		m_graph.newv(child_v);

	m_graph.newe(par_v, child_v);
}

void LinkRelations::remove(LinkID parent, LinkID child)
{
	using namespace graph;

	VertexID par_v = (VertexID)parent;
	VertexID child_v = (VertexID)child;

	if (!m_graph.hasv(par_v) || !m_graph.hasv(child_v))
		return;

	EdgeID e = m_graph.dir_edge(par_v, child_v);
	if (e != INVALID_E)
		m_graph.dele(e);
}

bool LinkRelations::is_contained_in(LinkID parent, LinkID child) const
{
	using namespace graph;

	VertexID par_v = (VertexID)parent;
	VertexID child_v = (VertexID)child;

	// Common case: check direct connection
	EdgeID e = m_graph.dir_edge(par_v, child_v);
	if (e != INVALID_E)
		return true;

	// Backtrack farther
	std::stack<VertexID> to_visit;
	to_visit.push(child_v);

	while (!to_visit.empty())
	{
		VertexID cur_child = to_visit.top();
		to_visit.pop();

		auto parents = m_graph.dir_neigh_r(cur_child);
		for (auto parent : parents)
		{
			if (parent == par_v)
				return true;

			to_visit.push(parent);
		}
	}

	return false;
}

void LinkRelations::unregister_link(LinkID link)
{
	using namespace graph;

	VertexID vid = (VertexID)link;
	m_graph.delv(vid);
}

std::vector<LinkID> LinkRelations::get_immediate_parents(LinkID link) const
{
	return get_immediate_porc_internal(link, &graph::Graph::dir_neigh_r);
}

std::vector<LinkID> LinkRelations::get_immediate_children(LinkID link) const
{
	return get_immediate_porc_internal(link, &graph::Graph::dir_neigh);
}

std::vector<LinkID> LinkRelations::get_parents(LinkID link, NetType type) const
{
	return get_porc_internal(link, type, &graph::Graph::dir_neigh_r);
}

std::vector<LinkID> LinkRelations::get_children(LinkID link, NetType type) const
{
	return get_porc_internal(link, type, &graph::Graph::dir_neigh);
}

std::vector<LinkID> LinkRelations::get_immediate_porc_internal(LinkID link,
	graph::VList(graph::Graph::* grafunc)(graph::VertexID) const) const
{
	using namespace graph;
	std::vector<LinkID> result;

	VertexID link_v = (VertexID)link;

	auto neighs = (m_graph.*grafunc)(link_v);
	for (auto neigh_v : neighs)
	{
		result.push_back((LinkID)neigh_v);
	}

	return result;
}

void LinkRelations::get_porc_internal(Link * link, NetType net, Node* sys,
	graph::VList(graph::Graph::* grafunc)(graph::VertexID) const, void * pout,
	const ThuncFunc& thunk) const
{
	using namespace graph;
	std::vector<void*>& out = *((std::vector<void*>*)pout);
	
	std::vector<LinkID> out_ids = get_porc_internal(link->get_id(), net, grafunc);
	for (auto id : out_ids)
	{
		Link* link = sys->get_link(id);
		out.push_back(thunk(link));
	}
}

std::vector<LinkID> LinkRelations::get_porc_internal(LinkID link, NetType net,
	graph::VList(graph::Graph::* grafunc)(graph::VertexID) const) const
{
	using namespace graph;

	std::vector<LinkID> result;

	VertexID firstv = (VertexID)link;

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
		LinkID curv_linkid = (LinkID)curv;
		if (curv_linkid.get_type() == net)
		{
			result.push_back(curv_linkid);
		}
	}

	return result;
}

void LinkRelations::prune(Node* dest)
{
	// Remove all vertices not present in dest
	std::vector<graph::VertexID> to_remove;
	for (auto v : m_graph.iter_verts)
	{
		auto linkid = (LinkID)v;

		if (!dest->get_link(linkid))
		{
			to_remove.push_back(v);
		}
	}

	for (auto v : to_remove)
	{
		m_graph.delv(v);
	}

	// (The edges were copied from the source graph. Some may have been pruned by delv)
}

void LinkRelations::reintegrate(LinkRelations& src)
{
	// Perform a union of the graphs
	m_graph.union_with(src.m_graph);
}


EndpointPair::EndpointPair()
	: in(nullptr), out(nullptr)
{
}


