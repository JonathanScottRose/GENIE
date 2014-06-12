#include <cassert>
#include <algorithm>
#include <string>
#include <fstream>
#include "graph.h"

using namespace ct;
using namespace ct::Graphs;

namespace ct
{
namespace Graphs
{
	template<class C, class T>
	bool Graph::IterContainer<C, T>::iterator::operator!= (const iterator& other) const
	{
		return it != other.it;
	}

	template<class C, class T>
	typename Graph::IterContainer<C, T>::iterator& Graph::IterContainer<C, T>::iterator::operator++()
	{
		it++;
		return *this;
	}

	template<class C, class T>
	const T& Graph::IterContainer<C, T>::iterator::operator*() const
	{
		return it->first;
	}

	template<>
	Graph::IterContainer<Graph::VContType, VertexID>::iterator 
		Graph::IterContainer<Graph::VContType, VertexID>::begin() const
	{
		return iterator(g, g.V.begin());
	}

	template<>
	 Graph::IterContainer<Graph::VContType, VertexID>::iterator
		Graph::IterContainer<Graph::VContType, VertexID>::end() const
	{
		return iterator(g, g.V.end());
	}

	template<>
	Graph::IterContainer<Graph::EContType, EdgeID>::iterator
	Graph::IterContainer<Graph::EContType, EdgeID>::begin() const
	{
		return iterator(g, g.E.begin());
	}

	template<>
	Graph::IterContainer<Graph::EContType, EdgeID>::iterator
	Graph::IterContainer<Graph::EContType, EdgeID>::end() const
	{
		return iterator(g, g.E.end());
	}

	template class Graph::IterContainer<Graph::VContType, VertexID>;
	template class Graph::IterContainer<Graph::EContType, EdgeID>;
}




void Graph::remove_e(VertexID vid, EdgeID eid)
{
	auto& v = getv(vid);
	auto& edges = v.edges;

	auto thisedge = [=](EdgeID eid2) { return eid == eid2; };
	edges.erase(std::remove_if(edges.begin(), edges.end(), thisedge), edges.end());
}
}

Graph::Vertex& Graph::getv(VertexID id)
{
	assert(V.count(id));
	return V[id];
}

const Graph::Vertex& Graph::getv(VertexID id) const
{
	assert(V.count(id));
	return V.at(id);
}

Graph::Edge& Graph::gete(EdgeID id)
{
	assert(E.count(id));
	return E[id];
}

const Graph::Edge& Graph::gete(EdgeID id) const
{
	assert(E.count(id));
	return E.at(id);
}

VertexID Graph::newv()
{
	auto ins = V.emplace(m_next_vid, Vertex());
	m_next_vid++;
	
	VertexID result = ins.first->first;

	return result;
}

EdgeID Graph::newe(VertexID v1, VertexID v2)
{
	auto ins = E.emplace(m_next_eid, Edge(v1, v2));
	m_next_eid++;

	EdgeID result = ins.first->first;

	getv(v1).edges.push_back(result);
	getv(v2).edges.push_back(result);
	
	return result;
}

EdgeID Graph::newe()
{
	return newe(INVALID_V, INVALID_V);
}

void Graph::connect(VertexID vid1, VertexID vid2, EdgeID eid)
{
	Edge& e = gete(eid);
	e.v1 = vid1;
	e.v2 = vid2;
	Vertex& v1 = getv(vid1);
	Vertex& v2 = getv(vid2);
	if (std::find(v1.edges.begin(), v1.edges.end(), eid) == v1.edges.end())
		v1.edges.push_back(eid);
	if (std::find(v2.edges.begin(), v2.edges.end(), eid) == v2.edges.end())
		v2.edges.push_back(eid);
}

VertexID Graph::otherv(EdgeID eid, VertexID self)
{
	Edge& e = gete(eid);
	
	assert(e.v1 == self || e.v2 == self);
	return (e.v1 == self) ? e.v2 : e.v1;
}

void Graph::set_otherv(EdgeID eid, VertexID self, VertexID newdest)
{
	Edge& e = gete(eid);

	assert(e.v1 == self || e.v2 == self);
	if (e.v1 == self) e.v2 = newdest;
	else if (e.v2 == self) e.v1 = newdest;
}

EList& Graph::edges(VertexID vid)
{
	Vertex& v = getv(vid);
	return v.edges;
}

const EList& Graph::edges(VertexID vid) const
{
	const Vertex& v = getv(vid);
	return v.edges;
}

VList Graph::neigh(VertexID vid)
{
	VList result;

	Vertex& v = getv(vid);
	for (auto& eid : v.edges)
	{
		VertexID oid = otherv(eid, vid);
		if (std::find(result.begin(), result.end(), oid) == result.end())
			result.push_back(oid);
	}

	return result;
}

VPair Graph::verts(EdgeID eid)
{
	Edge& e = gete(eid);
	return VPair(e.v1, e.v2);
}

void Graph::delv(VertexID vid)
{
	Vertex& v = V[vid];

	for (auto& eid : v.edges)
	{
		VertexID oid = otherv(eid, vid);
		remove_e(oid, eid);
		E.erase(eid);
	}

	V.erase(vid);
}

void Graph::dele(EdgeID eid)
{
	VPair vids = verts(eid);
	remove_e(vids.first, eid);
	remove_e(vids.second, eid);
	E.erase(eid);
}

EList Graph::edges(VertexID vid1, VertexID vid2)
{
	EList result;

	EList eids1 = edges(vid1);
	for (auto& eid : eids1)
	{
		if (otherv(eid, vid1) == vid2)
			result.push_back(eid);
	}

	return result;
}

EdgeID Graph::edge(VertexID vid1, VertexID vid2)
{
	auto ee = edges(vid1, vid2);
	assert(ee.size() < 2);
	return ee.empty()? INVALID_E : ee.front();
}

EList Graph::dir_edges(VertexID vid1, VertexID vid2)
{
	EList result;

	EList eids1 = edges(vid1);
	for (auto& eid : eids1)
	{
		VPair vs = verts(eid);
		if (vs.second == vid2)
			result.push_back(eid);
	}

	return result;
}

EdgeID Graph::redge(EdgeID eid)
{
	VPair v = verts(eid);
	return dir_edge(v.second, v.first);
}

EdgeID Graph::dir_edge(VertexID vid1, VertexID vid2)
{
	auto ee = dir_edges(vid1, vid2);
	assert(ee.size() < 2);
	return ee.empty() ? INVALID_E : ee.front();
}

void Graph::mergev(VertexID vidsrc, VertexID viddest)
{
	// delete all edges between src and dest
	// reassign src's remaining edges to dest
	// delete src

	EList common_edges = edges(vidsrc, viddest);
	for (auto& eid : common_edges)
	{
		dele(eid);
	}

	EList remaining_edges = edges(vidsrc);
	for (auto& eid : remaining_edges)
	{
		VertexID o = otherv(eid, vidsrc);
		set_otherv(eid, o, viddest);
		edges(viddest).push_back(eid);
	}

	V.erase(vidsrc);
}

Graph::IterContainer<Graph::VContType, VertexID> Graph::verts()
{
	return IterContainer<VContType, VertexID>(*this);
}

Graph::IterContainer<Graph::EContType, EdgeID> Graph::edges()
{
	return IterContainer<EContType, EdgeID>(*this);
}

void Graph::dump(const std::string& filename, const std::function<std::string(EdgeID)>& efunc)
{
	std::ofstream out(filename + ".dot");

	out << "digraph G {" << std::endl;

	for (auto& i : E)
	{
		EdgeID eid = i.first;
		Edge& e = i.second;

		std::string ltxt;
		if (efunc)
		{
			ltxt = " [label=\"" + efunc(eid) + "\"]";
		}

		out << std::to_string(e.v1) << " -> " << std::to_string(e.v2)
			<< ltxt << ";"
			<< std::endl;
	}

	out << "}" << std::endl;
}