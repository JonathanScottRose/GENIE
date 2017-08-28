#include "pch.h"
#include "flow.h"
#include "graph.h"
#include "net_rs.h"
#include "port_rs.h"
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
				return node->is_link_internal(lnk);
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
	: m_is_manual(false), m_id(INVALID), m_disable_opt(false)
{
}

void DomainRS::add_link(LinkID link)
{
	m_links.push_back(link);
}

const DomainRS::Links& DomainRS::get_links() const
{
	return m_links;
}

void DomainRS::add_transmission(unsigned xmis)
{
	m_xmis.push_back(xmis);
}

const DomainRS::Transmissions & DomainRS::get_transmissions() const
{
	return m_xmis;
}

unsigned DomainRS::get_port_domain(PortRS * port, NodeSystem * context)
{
	auto result = DomainRS::INVALID;

	auto ep = port->get_endpoint(NET_RS_LOGICAL, port->get_effective_dir(context));
	if (ep)
	{
		auto link = static_cast<LinkRSLogical*>(ep->get_link0());
		if (link)
			result = link->get_domain_id();
	}

	return result;
}

//
// FlowStateOuter
//

FlowStateOuter::RSDomains& FlowStateOuter::get_rs_domains()
{
	return m_rs_domains;
}

DomainRS* FlowStateOuter::get_rs_domain(unsigned id)
{
	if (id >= m_rs_domains.size())
		return nullptr;

	auto& result = m_rs_domains[id];
	if (result.get_id() == DomainRS::INVALID)
		return nullptr;
	
	return &result;
}

DomainRS& FlowStateOuter::new_rs_domain(unsigned id)
{
	if (id >= m_rs_domains.size())
		m_rs_domains.resize(id + 1);

	auto& result = m_rs_domains[id];
	result.set_id(id);
	return result;
}

TransmissionID FlowStateOuter::new_transmission()
{
	TransmissionID result = m_transmissions.size();

	m_transmissions.emplace_back(TransmissionInfo{ {} });

	return result;
}

void FlowStateOuter::add_link_to_transmission(TransmissionID xmis, LinkID link)
{
	auto& xm = m_transmissions[xmis];

	xm.links.push_back(link);
	
	assert(m_link_to_xmis.count(link) == 0);
	m_link_to_xmis[link] = xmis;
}

const std::vector<LinkID>& FlowStateOuter::get_transmission_links(TransmissionID xmis)
{
	return m_transmissions[xmis].links;
}

unsigned FlowStateOuter::get_n_transmissions() const
{
	return m_transmissions.size();
}

TransmissionID genie::impl::flow::FlowStateOuter::get_transmission_for_link(LinkID link)
{
	auto it = m_link_to_xmis.find(link);
	assert(it != m_link_to_xmis.end());
	return it->second;
}
	



