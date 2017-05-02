#include "pch.h"
#include "genie_priv.h"
#include "vlog_write.h"
#include "hdl_elab.h"
#include "graph.h"
#include "flow.h"
#include "node_system.h"
#include "node_split.h"
#include "node_merge.h"
#include "net_conduit.h"
#include "net_rs.h"
#include "net_topo.h"
#include "port_conduit.h"
#include "port_rs.h"

using namespace genie::impl;
using genie::Exception;

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
		auto fstate = sys->get_flow_state_outer();
		for (auto& it : link_to_eid)
		{
			auto link = dynamic_cast<LinkRSLogical*>(it.first);
			assert(link);
			auto eid = it.second;

			assert(eid_to_domain.count(eid));
			unsigned dom_id = eid_to_domain[eid];

			auto dom = fstate->get_rs_domain(dom_id);
			if (!dom)
			{
				dom = &fstate->new_rs_domain(dom_id);
				// Name domain after the first src port seen
				dom->set_name(link->get_src()->get_hier_path(sys));
			}

			dom->add_link(link);
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

			auto fstate = sys->get_flow_state_outer();

			for (auto port : { src_port, sink_port })
			{
				if (port)
				{
					unsigned dom_id = port->get_domain_id();
					fstate->get_rs_domain(dom_id)->set_is_manual(true);
				}
			}
		}
	}

	void rs_print_domain_stats(NodeSystem* sys)
	{
		namespace log = genie::log;

		auto fstate = sys->get_flow_state_outer();
		auto& domains = fstate->get_rs_domains();

		if (domains.size() == 0)
			return;

		log::info("System %s: found %u transmission domains", 
			sys->get_name().c_str(), domains.size());

		for (auto& dom : fstate->get_rs_domains())
		{
			if (dom.get_is_manual())
			{
				log::info("  Domain %s has manual topology",
					dom.get_name().c_str());
			}
		}
	}

	void make_crossbar_topo(NodeSystem* sys)
	{
		// Get all logical RS links
		auto& logical_links = sys->get_links(NET_RS_LOGICAL);

		// For each original topo source, and sink record:
		struct Entry
		{
			// The frontier port: equal to either the original source/sink,
			// or a split/merge node
			HierObject* head;
			// The remote destinations that this connects to
			std::unordered_set<HierObject*> remotes;
		};

		std::unordered_map<HierObject*, Entry> srces, sinks;

		// Initialize sources, sinks
		for (auto logical_link : logical_links)
		{
			auto rs_src = logical_link->get_src();
			auto rs_sink = logical_link->get_sink();

			// Get or create entries for src/sink
			Entry& en_src = srces[rs_src];
			Entry& en_sink = sinks[rs_sink];

			en_src.head = rs_src;
			en_src.remotes.insert(rs_sink);

			en_sink.head = rs_sink;
			en_sink.remotes.insert(rs_src);
		}

		// Create split/merge nodes
		for (auto& src : srces)
		{
			HierObject* orig_src = src.first;
			Entry& en = src.second;

			if (en.remotes.size() > 1)
			{
				std::string sp_name = util::str_con_cat("sp", orig_src->get_hier_path(sys));
				std::replace(sp_name.begin(), sp_name.end(), HierObject::PATH_SEP, '_');

				auto sp = new NodeSplit();
				sp->set_name(sp_name);
				sys->add_child(sp);

				sys->connect(orig_src, sp, NET_TOPO);

				// Advance head to output of split node
				en.head = sp;
			}
		}

		for (auto& sink : sinks)
		{
			HierObject* orig_sink = sink.first;
			Entry& en = sink.second;

			if (en.remotes.size() > 1)
			{
				std::string mg_name = util::str_con_cat("mg", orig_sink->get_hier_path(sys));
				std::replace(mg_name.begin(), mg_name.end(), HierObject::PATH_SEP, '_');

				auto mg = new NodeMerge();
				mg->set_name(mg_name);
				sys->add_child(mg);

				sys->connect(mg, orig_sink, NET_TOPO);

				// Advance head to input of merge node
				en.head = mg;
			}
		}

		// Connect heads
		for (auto& src : srces)
		{
			Entry& src_en = src.second;
			HierObject* src_head = src_en.head;

			for (auto sink : src_en.remotes)
			{
				// Get the entry for the sink, retrieve head
				Entry& sink_en = sinks[sink];
				HierObject* sink_head = sink_en.head;

				// Connect
				sys->connect(src_head, sink_head, NET_TOPO);
			}
		}
	}

	void topo_do_routing(NodeSystem* sys)
	{
		using namespace graph;

		auto& rs_links = sys->get_links(NET_RS_LOGICAL);

		// Turn the RS logical network into a graph.
		// Maintain: vertexid<->port, edgeid->link mappings
		E2Attr<Link*> eid_to_link;
		V2Attr<HierObject*> vid_to_port;
		Attr2V<HierObject*> port_to_vid;
		Graph topo_g = flow::net_to_graph(sys, NET_TOPO, true, 
			&vid_to_port, &port_to_vid,
			&eid_to_link, nullptr, nullptr);

		//topo_g.dump("debug", [=](VertexID v) {return topo_vid_to_port.at(v)->get_hier_path();});

		for (auto& rs_link : rs_links)
		{
			// Get endpoints
			auto src = rs_link->get_src();
			auto sink = rs_link->get_sink();

			// Find a route
			bool result = false;
			EList route_edges;

			assert(port_to_vid.count(src) && port_to_vid.count(sink));
			VertexID v_src = port_to_vid[src];
			VertexID v_sink = port_to_vid[sink];
			result = graph::dijkstra(topo_g, v_src, v_sink, nullptr, nullptr, &route_edges);

			if (!result)
			{
				throw Exception("No route found from " + src->get_hier_path() + " to " +
					sink->get_hier_path());
			}
			
			// Walk the edges and associate the RS link with each constituent TOPO link
			auto link_rel = sys->get_link_relations();
			for (auto& route_edge : route_edges)
			{
				Link* route_link = eid_to_link[route_edge];
				link_rel->add(rs_link, route_link);
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

	void do_auto_domain(NodeSystem* sys, unsigned dom_id)
	{
		NodeSystem* snapshot = sys->create_snapshot(dom_id);

		// Apply crossbar topology and save snapshot
		make_crossbar_topo(snapshot);
		NodeSystem* topo_snapshot = snapshot->clone();

		//
		// Outer loop starts here
		//

		// Do automatic routing
		topo_do_routing(snapshot);
		
		// Take the xbar topo system through the inner flow
		flow::do_inner(snapshot);
		
		// Merge changes
		sys->reintegrate_snapshot(snapshot);
		delete snapshot;

		//
		// Outer loop ends here
		//

		delete topo_snapshot;
	}

	void do_manual_domain(NodeSystem* sys, unsigned dom_id)
	{
	}

	void do_all_domains(NodeSystem* sys)
	{
		// Get IDs of non-manual domains
		auto fstate = sys->get_flow_state_outer();
		std::vector<unsigned> auto_domains, manual_domains;

		for (auto& dom : fstate->get_rs_domains())
		{
			if (dom.get_is_manual())
				manual_domains.push_back(dom.get_id());
			else
				auto_domains.push_back(dom.get_id());
		}

		// Do automatic domains first
		for (auto dom_id : auto_domains)
		{
			do_auto_domain(sys, dom_id);
		}

		for (auto dom_id : manual_domains)
		{
			do_manual_domain(sys, dom_id);
		}
	}

    void do_system(NodeSystem* sys)
    {
		sys->resolve_params();
		sys->set_flow_state_outer(new flow::FlowStateOuter());

		rs_assign_domains(sys);
		rs_find_manual_domains(sys);
		rs_print_domain_stats(sys);

		do_all_domains(sys);

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


