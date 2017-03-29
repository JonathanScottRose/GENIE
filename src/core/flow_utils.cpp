#include "pch.h"
#include "flow.h"
#include "graph.h"
#include "node_system.h"

using namespace genie::impl::flow;
using namespace genie::impl;
using namespace genie::impl::graph;

Graph flow::net_to_graph(NodeSystem* sys, NetType ntype,
	bool include_internal,
	V2Attr<HierObject*>* v_to_obj,
	Attr2V<HierObject*>* obj_to_v,
	E2Attr<Link*>* e_to_link,
	Attr2E<Link*>* link_to_e,
	const flow::N2GRemapFunc& remap)
{
	Graph result;

	// Gather all edges from system
	auto links = sys->get_links(ntype);

	// Gather all internal edges from nodes, if requested
	if (include_internal)
	{
		for (auto node : sys->get_nodes())
		{
			auto links_int = node->get_links(ntype);
			std::copy_if(links_int.begin(), links_int.end(), 
				std::back_inserter(links), [=](Link* lnk)
			{
				return sys->is_link_internal(lnk);
			});
		}
	}

	// Whether or not the caller needs one, we require an obj->v map.
	// If the caller doesn't provide one, allocate (and then later free) a local one.
	bool local_map = !obj_to_v;
	if (local_map)
		obj_to_v = new Attr2V<HierObject*>;

	for (auto link : links)
	{
		// Get link's endpoints
		HierObject* src = link->get_src();
		HierObject* sink = link->get_sink();

		// Remap if a remap function was provided
		if (remap)
		{
			src = remap(src);
			sink = remap(sink);
		}

		// Create (or retrieve) associated vertex IDs for endpoints.
		// Update reverse association if caller requested one.
		VertexID src_v, sink_v;

		for (auto& p : { std::make_pair(&src_v, src), std::make_pair(&sink_v, sink) })
		{
			// src_v or sink_v
			VertexID* pv = p.first;
			// src or sink
			HierObject* obj = p.second;

			auto it = obj_to_v->find(obj);
			if (it == obj_to_v->end())
			{
				*pv = result.newv();
				obj_to_v->emplace(obj, *pv);
				if (v_to_obj) v_to_obj->emplace(*pv, obj);
			}
			else
			{
				*pv = it->second;
			}
		}

		// Create edge and update mappings if requested
		EdgeID e = result.newe(src_v, sink_v);
		if (e_to_link) e_to_link->emplace(e, link);
		if (link_to_e) link_to_e->emplace(link, e);
	}

	if (local_map)
		delete obj_to_v;

	return result;
}

//
// DomainRS
//

DomainRS::DomainRS()
	: m_is_manual(false)
{
}

void DomainRS::add_link(LinkRSLogical* link)
{
	m_links.push_back(link);
}

const DomainRS::Links& DomainRS::get_links() const
{
	return m_links;
}

//
// NodeFlowState
//

const NodeFlowState::RSDomains& NodeFlowState::get_rs_domains()
{
	return m_rs_domains;
}

DomainRS& NodeFlowState::get_rs_domain(unsigned id)
{
	assert(id < m_rs_domains.size());
	return m_rs_domains[id];
}

DomainRS& NodeFlowState::new_rs_domain(unsigned id)
{
	if (id >= m_rs_domains.size())
		m_rs_domains.resize(id + 1);

	return m_rs_domains[id];
}


