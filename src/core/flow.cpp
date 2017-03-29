#include "pch.h"
#include "genie_priv.h"
#include "vlog_write.h"
#include "hdl_elab.h"
#include "graph.h"
#include "flow.h"
#include "node_system.h"
#include "net_conduit.h"
#include "net_rs.h"
#include "net_topo.h"
#include "port_conduit.h"
#include "port_rs.h"

using namespace genie::impl;

namespace
{
	void rs_assign_domains(NodeSystem* sys)
	{
		using namespace graph;

		// We care about port->vid mapping and link->eid mapping
		Attr2E<Link*> link_to_eid;
		Attr2V<HierObject*> port_to_vid;
		Graph rs_g = flow::net_to_graph(sys, NET_RS_LOGICAL, false, 
			nullptr, &port_to_vid, nullptr, &link_to_eid);

		// Identify connected components (domains).
		// Capture vid->domainid mapping and eid->domainid mapping
		V2Attr<unsigned> vid_to_domain;
		E2Attr<unsigned> eid_to_domain;
		connected_comp(rs_g, &vid_to_domain, &eid_to_domain);

		// Now assign each port its domain ID.
		for (auto& it : port_to_vid)
		{
			auto port = dynamic_cast<PortRS*>(it.first);
			assert(port);
			auto vid = it.second;

			// Look up the domain for this vid
			assert(vid_to_domain.count(vid));
			unsigned domain = vid_to_domain[vid];

			port->set_domain_id(domain);
		}

		// Create the domain structures and sort the links into the domains.
		auto fstate = sys->get_flow_state();
		for (auto& it : link_to_eid)
		{
			auto link = dynamic_cast<LinkRSLogical*>(it.first);
			assert(link);
			auto eid = it.second;

			assert(eid_to_domain.count(eid));
			unsigned dom_id = eid_to_domain[eid];

			bool new_domain = dom_id >= fstate->get_rs_domains().size();
			auto& dom = new_domain ?
				fstate->new_rs_domain(dom_id) : fstate->get_rs_domain(dom_id);

			dom.add_link(link);

			// Name domain after the first src port seen
			if (new_domain)
				dom.set_name(link->get_src()->get_hier_path(sys));
		}
	}

	void rs_find_manual_domains(NodeSystem* sys)
	{
		// Look for all existing Topo links.
		// This early in the flow, all such links were manually-created.
		// Then, mark the domains of the endpoints as 'manual'.

		for (auto link : sys->get_links(NET_TOPO))
		{
			auto src_port = dynamic_cast<PortRS*>(link->get_src());
			auto sink_port = dynamic_cast<PortRS*>(link->get_sink());

			auto fstate = sys->get_flow_state();

			for (auto port : { src_port, sink_port })
			{
				if (port)
				{
					unsigned dom_id = port->get_domain_id();
					fstate->get_rs_domain(dom_id).set_is_manual(true);
				}
			}
		}
	}

	void rs_print_domain_stats(NodeSystem* sys)
	{
		namespace log = genie::log;

		auto fstate = sys->get_flow_state();
		auto& domains = fstate->get_rs_domains();

		if (domains.size() == 0)
			return;

		log::info("Found %u transmission domains", domains.size());

		for (auto& dom : fstate->get_rs_domains())
		{
			if (dom.get_is_manual())
			{
				log::info("  Domain %s has manual topology",
					dom.get_name().c_str());
			}
		}
	}

	void connect_conduits(NodeSystem* sys)
	{
		// Find conduit links
		auto links = sys->get_links(NET_CONDUIT);

		for (auto cnd_link : links)
		{
			auto cnd_src = dynamic_cast<PortConduit*>(cnd_link->get_src());
			auto cnd_sink = dynamic_cast<PortConduit*>(cnd_link->get_sink());

			// Get the source's subports
			auto src_subs = cnd_src->get_subports();

			// For each src subport, find the subport on the sink with a matching tag
			for (auto src_sub : src_subs)
			{
				auto& tag = src_sub->get_tag();

				// If one is not found, continue but throw warnings later
				auto sink_sub = cnd_sink->get_subport(tag);
				if (!sink_sub)
				{
					genie::log::warn("connecting %s to %s: %s is missing field '%s'",
						cnd_src->get_hier_path().c_str(),
						cnd_sink->get_hier_path().c_str(),
						cnd_sink->get_hier_path().c_str(),
						tag.c_str());
					continue;
				}

				// Does the sub->sub direction need to be reversed?
				bool swapdir = src_sub->get_effective_dir(sys) == Port::Dir::IN;
				if (swapdir)
					std::swap(src_sub, sink_sub);

				sys->connect(src_sub, sink_sub, NET_CONDUIT_SUB);
			}
		}
	}

    void do_system(NodeSystem* sys)
    {
		sys->resolve_params();
		sys->set_flow_state(new flow::NodeFlowState);

		rs_assign_domains(sys);
		rs_find_manual_domains(sys);
		rs_print_domain_stats(sys);

		connect_conduits(sys);
		hdl::elab_system(sys);
		hdl::write_system(sys);
    }
}

void genie::do_flow()
{
    auto systems = get_systems();

    for (auto& sys : systems)
    {
        do_system(sys);
    }
}


