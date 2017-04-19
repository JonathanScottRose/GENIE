#include "pch.h"
#include "flow.h"
#include "graph.h"
#include "net_rs.h"
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
	: m_is_manual(false), m_id(INVALID)
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

NodeFlowState::NodeFlowState(const NodeFlowState* that, const NodeSystem* old_sys,
	NodeSystem * new_sys)
{
	// Redo references

	for (auto& old_dom : that->m_rs_domains)
	{
		auto& new_dom = this->new_rs_domain(old_dom.get_id());

		for (auto& old_link: old_dom.get_links())
		{
			auto src_path = old_link->get_src()->get_hier_path(old_sys);
			auto sink_path = old_link->get_sink()->get_hier_path(old_sys);

			auto new_src = dynamic_cast<HierObject*>(new_sys->get_child(src_path));
			auto new_sink = dynamic_cast<HierObject*>(new_sys->get_child(sink_path));
			if (!new_src || !new_sink)
				continue;

			// Find the equivalent link in the new system. It might not exist, even
			// if the endpoints do, because it belongs to a different domain. This is okay.
			auto new_links = new_sys->get_links(new_src, new_sink, NET_RS_LOGICAL);
			if (new_links.empty())
				continue;

			// But if it DOES exist, there should just be one. Can expand on this later.
			assert(new_links.size() == 1);
			
			new_dom.add_link(static_cast<LinkRSLogical*>(new_links.front()));
		}
	}
}

void NodeFlowState::reintegrate(NodeFlowState & src)
{
	// TODO: move data from src that could have been added.
	// Nothing for now.
}

const NodeFlowState::RSDomains& NodeFlowState::get_rs_domains() const
{
	return m_rs_domains;
}

DomainRS* NodeFlowState::get_rs_domain(unsigned id)
{
	if (id >= m_rs_domains.size())
		return nullptr;

	auto& result = m_rs_domains[id];
	if (result.get_id() == DomainRS::INVALID)
		return nullptr;
	
	return &result;
}

DomainRS& NodeFlowState::new_rs_domain(unsigned id)
{
	if (id >= m_rs_domains.size())
		m_rs_domains.resize(id + 1);

	auto& result = m_rs_domains[id];
	result.set_id(id);
	return result;
}


