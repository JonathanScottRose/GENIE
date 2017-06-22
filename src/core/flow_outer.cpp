#include "pch.h"
#include "genie_priv.h"
#include "vlog_write.h"
#include "hdl_elab.h"
#include "graph.h"
#include "flow.h"
#include "node_system.h"
#include "node_split.h"
#include "node_merge.h"
#include "node_user.h"
#include "net_conduit.h"
#include "net_rs.h"
#include "net_topo.h"
#include "net_clockreset.h"
#include "port_conduit.h"
#include "port_rs.h"
#include "topo_optimize.h"

using namespace genie::impl;
using genie::Exception;
using flow::FlowStateOuter;
using flow::TransmissionID;

namespace
{
	NodeSystem * create_snapshot(NodeSystem* sys, FlowStateOuter& fstate,
		unsigned dom_id)
	{
		std::unordered_set<HierObject*> dom_nodes;
		std::vector<Link*> dom_links;

		// Gather all RS logical links for the given domain
		auto& dom_rs_link_ids = fstate.get_rs_domain(dom_id)->get_links();

		// Gather all Nodes that these RS Links touch
		for (auto rs_link_id : dom_rs_link_ids)
		{
			auto rs_link = static_cast<LinkRSLogical*>(sys->get_link(rs_link_id));
			dom_links.push_back(rs_link);

			for (auto obj : { rs_link->get_src(), rs_link->get_sink() })
			{
				// If the link connects a Port, get the Node of that Port
				if (auto port = dynamic_cast<Port*>(obj))
				{
					// Ignore top-level ports (don't add the system itself info this set)
					auto node = port->get_node();
					if (node != sys)
						dom_nodes.insert(node);
				}
			}
		}

		// Add clock and reset links to the preserve set
		for (auto net : { NET_CLOCK, NET_RESET })
		{
			auto add_links = sys->get_links(net);
			dom_links.insert(dom_links.end(), add_links.begin(), add_links.end());
		}

		return sys->create_snapshot(dom_nodes, dom_links);
	}

	void init_elemental_transmission_specs(NodeSystem* sys)
	{
		// Apply RS Port packet size/importance to individual RS logical links
		auto rs_links = sys->get_links_casted<LinkRSLogical>(NET_RS_LOGICAL);
		for (auto link : rs_links)
		{
			auto src = static_cast<PortRS*>(link->get_src());

			// If link's version is unset, use default size associated with source port
			if (link->get_packet_size() == LinkRSLogical::PKT_SIZE_UNSET)
				link->set_packet_size(src->get_default_packet_size());

			// Do the same for importance
			if (link->get_importance() == LinkRSLogical::IMPORTANCE_UNSET)
				link->set_importance(src->get_default_importance());
		}
	}

	void init_user_protocols(NodeSystem* sys)
	{
		// Search (all user nodes) + (this system)'s ports		
		auto nodes = sys->get_children_by_type<NodeUser, std::vector<Node*>>();
		nodes.push_back(sys);

		// Now scan these nodes for RS ports that have at least one logical
		// link attached to them
		std::vector<PortRS*> ports;
		for (auto node : nodes)
		{
			auto node_ports = node->get_children<PortRS>([=](const PortRS* p)
			{
				return p->get_endpoint(NET_RS_LOGICAL, 
					p->get_effective_dir(sys))->is_connected();
			});
			ports.insert(ports.end(), node_ports.begin(), node_ports.end());
		}

		// Do protocol processing
		for (auto port : ports)
		{
			// Do protocol and backpressure processing
			auto& proto = port->get_proto();
			auto& bp_status = port->get_bp_status();

			// Initialize backpressure to off, and turn on if
			// ready signal is discovered below
			bp_status.force_disable();

			// Browse through signal roles
			for (auto& rb : port->get_role_bindings())
			{
				auto& role = rb.role;
				auto& hdl_bnd = rb.binding;

				if (hdl_bnd.get_slices() > 1)
				{
					throw Exception(port->get_hier_path() + " " + role.type.to_string() +
						": multi-dimensional HDL signal bindings not supported");
				}

				if (role.type == PortRS::ADDRESS)
				{
					proto.add_terminal_field({ FIELD_USERADDR, hdl_bnd.get_bits() }, role);
				}
				else if (role.type == PortRS::EOP)
				{
					proto.add_terminal_field({ FIELD_EOP, 1 }, role);
				}
				else if (role.type == PortRS::DATA || role.type == PortRS::DATABUNDLE)
				{
					// Create a domain field
					unsigned domain = flow::DomainRS::get_port_domain(port, sys);
					assert(domain != flow::DomainRS::INVALID);

					FieldID f_id(FIELD_USERDATA, role.tag, domain);

					// Create a concretely-sized instance of the field using the
					// bound HDL port
					FieldInst f_inst(f_id, hdl_bnd.get_bits());

					proto.add_terminal_field(f_inst, role);
				}
				else if (role.type == PortRS::READY)
				{
					bp_status.force_enable();
				}
			}
		}
	}

	void rs_assign_domains(NodeSystem* sys, FlowStateOuter& fstate)
	{
		using namespace graph;

		// We care about link->eid mapping and port->vid mapping
		Attr2E<Link*> link_to_eid;
		Attr2V<HierObject*> port_to_vid;
		Graph rs_g = flow::net_to_graph(sys, NET_RS_LOGICAL, false, 
			nullptr, &port_to_vid, nullptr, &link_to_eid);

		// Add internal links from usernodes
		for (auto node : sys->get_children_by_type<NodeUser>())
		{
			// Look for existing internal links
			auto int_links = node->get_links(NET_RS_PHYS);

			// Try to map the link's ports to vertices from the above graph.
			// If they exist, connect them
			for (auto link : int_links)
			{
				auto src_v_it = port_to_vid.find(link->get_src());
				auto sink_v_it = port_to_vid.find(link->get_sink());
				if (src_v_it != port_to_vid.end() &&
					sink_v_it != port_to_vid.end())
				{
					rs_g.newe(src_v_it->second, sink_v_it->second);
				}
			}
		}

		// Identify connected components (domains).
		// Capture vid->domainid mapping and eid->domainid mapping
		V2Attr<unsigned> vid_to_domain;
		E2Attr<unsigned> eid_to_domain;
		connected_comp(rs_g, &vid_to_domain, &eid_to_domain);

		// Create the domain structures and sort the links into the domains.
		for (auto& it : link_to_eid)
		{
			auto link = static_cast<LinkRSLogical*>(it.first);
			auto eid = it.second;

			assert(eid_to_domain.count(eid));
			unsigned dom_id = eid_to_domain[eid];

			auto dom = fstate.get_rs_domain(dom_id);
			if (!dom)
			{
				dom = &fstate.new_rs_domain(dom_id);
				// Name domain after the first src port seen
				dom->set_name(link->get_src()->get_hier_path(sys));
			}

			dom->add_link(link->get_id());
			link->set_domain_id(dom_id);
		}
	}

	void rs_create_transmissions(NodeSystem* sys, FlowStateOuter& fstate)
	{
		// Go through all flows (logical RS links).
		// Bin them by source
		auto links = sys->get_links(NET_RS_LOGICAL);

		std::unordered_map<HierObject*, std::vector<LinkRSLogical*>> bin_by_src;

		for (auto link : links)
		{
			auto src = link->get_src();
			bin_by_src[src].push_back(static_cast<LinkRSLogical*>(link));
		}

		// Within each source bin, bin again by source address. 
		// Each of these bins is a transmission
		unsigned flow_id = 0;
		for (auto src_bin : bin_by_src)
		{
			std::unordered_map<unsigned, std::vector<LinkRSLogical*>> bin_by_addr;
			for (auto link : src_bin.second)
			{
				bin_by_addr[link->get_src_addr()].push_back(link);
			}

			for (auto addr_bin : bin_by_addr)
			{
				// Create the transmission
				TransmissionID xmis_id = fstate.new_transmission();

				// Add links to the transmission
				for (auto link : addr_bin.second)
				{
					fstate.add_link_to_transmission(xmis_id, link->get_id());
				}

				// Add transmission to domain
				unsigned dom_id = addr_bin.second.front()->get_domain_id();
				auto dom = fstate.get_rs_domain(dom_id);
				assert(dom);
				dom->add_transmission(xmis_id);
			}
		}
	}

	void rs_find_manual_domains(NodeSystem* sys, FlowStateOuter& fstate)
	{
		// Look for all existing Topo links.
		// This early in the flow, all such links were manually-created.
		// Then, mark the domains of the endpoints as 'manual'.

		for (auto link : sys->get_links(NET_TOPO))
		{
			auto src_port = dynamic_cast<PortRS*>(link->get_src());
			auto sink_port = dynamic_cast<PortRS*>(link->get_sink());

			for (auto port : { src_port, sink_port })
			{
				if (port)
				{
					unsigned dom_id = flow::DomainRS::get_port_domain(port, sys);
					assert(dom_id != flow::DomainRS::INVALID);
					fstate.get_rs_domain(dom_id)->set_is_manual(true);
				}
			}
		}
	}

	void rs_print_domain_stats(NodeSystem* sys, FlowStateOuter& fstate)
	{
		namespace log = genie::log;

		auto& domains = fstate.get_rs_domains();

		if (domains.size() == 0)
			return;

		log::info("System %s: found %u transmission domains", 
			sys->get_name().c_str(), domains.size());

		for (auto& dom : domains)
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
		auto logical_links = sys->get_links(NET_RS_LOGICAL);

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

		auto rs_links = sys->get_links(NET_RS_LOGICAL);

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
			auto& link_rel = sys->get_link_relations();
			for (auto& route_edge : route_edges)
			{
				Link* route_link = eid_to_link[route_edge];
				link_rel.add(rs_link->get_id(), route_link->get_id());
			}
		}
	}

	AreaMetrics measure_impl_area(NodeSystem* sys)
	{
		AreaMetrics result;

		for (auto node : sys->get_nodes())
		{
			result += node->annotate_area();
		}

		return result;
	}

	NodeSystem* do_auto_domain(NodeSystem * snapshot, FlowStateOuter& fstate, unsigned dom_id)
	{
		// A pair representing a "system configuration":
		// a topology-only system and the full system that it implements/elaborates into.
		// It also holds the area usage of the implementation.
		// Our goal is to find the best such pair!
		struct SysConfig
		{
			NodeSystem* topo = nullptr;
			NodeSystem* impl = nullptr;
			AreaMetrics area;
		};

		// The current 'best' sysconfig: initialized with a crossbar
		// topology and the implementation that results from it.
		SysConfig best_config;

		// Take the given system snapshot, which contains just the domain we want.
		// It has only logical links. Give it a crossbar topology.
		best_config.topo = snapshot;
		make_crossbar_topo(best_config.topo);

		// Measure contention for crossbar topology
		//topo_opt::ContentionMap xbar_contention = 
			//topo_opt::measure_initial_contention(best_config.topo, fstate);

		// Implement the initial crossbar topology and measure its area
		best_config.impl = best_config.topo->clone();
		topo_do_routing(best_config.impl);
		flow::do_inner(best_config.impl, dom_id, &fstate);
		best_config.area = measure_impl_area(best_config.impl);

		//
		// Outer loop starts here
		//
		for (bool outer_loop_done = false; !outer_loop_done; )
		{
			outer_loop_done = true;

			// Create next candidate topology from best_config.topo
			// If candidate is acceptable, implement it and measure its area.
			// Two possibilities:
			//  1) Candidate has worse area: ignore and dispose of candidate config
			//  2) Candidate has better area: candidate replaces best, dispose of old best, 
			//     set outer_loop_done = false

			// result: outer_loop_done will eventually remain true because no more acceptable
			// AND better candidates will be found than the current best
		}
	
		// Return the best possible implementation of the original
		// domain snapshot
		delete best_config.topo;
		return best_config.impl;
	}

	NodeSystem* do_manual_domain(NodeSystem* snapshot, FlowStateOuter& fstate, unsigned dom_id)
	{
		// Do automatic routing on the existing manual topology
		topo_do_routing(snapshot);

		// Do the inner flow just once
		flow::do_inner(snapshot, dom_id, &fstate);

		return snapshot;
	}

	void do_all_domains(NodeSystem* sys, FlowStateOuter& fstate)
	{
		for (auto& dom : fstate.get_rs_domains())
		{
			auto dom_id = dom.get_id();

			// Create snapshot of original system, just containing
			// the current domain and all the nodes connected to it.
			NodeSystem* in_snapshot = create_snapshot(sys, fstate, dom_id);
			NodeSystem* out_snapshot = nullptr;

			if (dom.get_is_manual())
			{
				out_snapshot = do_manual_domain(in_snapshot, fstate, dom_id);
			}
			else
			{
				out_snapshot = do_auto_domain(in_snapshot, fstate, dom_id);
			}

			// Integrate the fleshed-out domain into the master system
			sys->reintegrate_snapshot(out_snapshot);
			delete out_snapshot;
		}
	}

	void process_latency_queries(NodeSystem* sys)
	{
		auto& queries = sys->get_spec().latency_queries;
		auto& link_rel = sys->get_link_relations();

		for (auto& query : queries)
		{
			unsigned chain_latency = 0;

			// Current and last logical link IDs in chain, going backwards
			auto log_it = query.chain_links.rbegin();
			auto log_it_end = query.chain_links.rend();

			LinkID cur_log_id = *log_it;
			Link* cur_log = sys->get_link(cur_log_id);

			// The source of the current logical link (aka the end of traversal,
			// since we're going backwards)
			HierObject* cur_log_src = cur_log->get_src();

			// Current external physical link.
			Link* cur_ext_phys =
				cur_log->get_sink()->get_endpoint(NET_RS_PHYS, Port::Dir::IN)->get_link0();

			while (true)
			{
				// Get source of current physical link
				HierObject* cur_phys_src = cur_ext_phys->get_src();

				// Reached beginning of logical link?
				if (cur_phys_src == cur_log_src)
				{
					// Advance to next logical link
					log_it++;

					// Reached end of chain?
					if (log_it == log_it_end)
					{
						// done
						break;
					}
					else
					{
						// Update things related to current logical link	
						cur_log_id = *log_it;
						cur_log = sys->get_link(cur_log_id);
						cur_log_src = cur_log->get_src();
					}
				}

				// Traverse backwards through node, get latency. Examine all candidates
				// and find the one that continues with the correct logical link
				for (auto int_link :
					cur_phys_src->get_endpoint(NET_RS_PHYS, Port::Dir::IN)->links())
				{
					// Get the external physical link that feeds this interal link
					Link* next_ext_phys = int_link->get_src_ep()->get_sibling()->get_link0();

					// Does it exist, and does it continue the logical link we want?
					if (next_ext_phys &&
						link_rel.is_contained_in(cur_log_id, next_ext_phys->get_id()))
					{
						// Use it: update latency, and update cur_ext_phys to continue traversal
						cur_ext_phys = next_ext_phys;
						auto cur_phys_int = static_cast<LinkRSPhys*>(int_link);
						chain_latency += cur_phys_int->get_latency();
						break;
					}
				}
			} // end while(true)

			// chain_latency is set. create a system HDL parameter to hold it
			sys->set_int_param(query.param_name, (int)chain_latency);
		} // end foreach query
	}

	void resolve_size_params(NodeSystem* sys)
	{
		// Make HDL port sizes/bindings in the system constant by
		// resolving parameter expressions.
		//
		// Do this for the system node itself, as well as user nodes.

		auto resolv_nodes = 
			sys->get_children_by_type<NodeUser, std::vector<Node*>>();
		resolv_nodes.push_back(sys);

		for (auto node : resolv_nodes)
			node->resolve_size_params();
	}

    void do_system(NodeSystem* sys)
    {
		FlowStateOuter fstate;

		resolve_size_params(sys);

		rs_assign_domains(sys, fstate);
		rs_create_transmissions(sys, fstate);
		rs_find_manual_domains(sys, fstate);
		rs_print_domain_stats(sys, fstate);

		init_user_protocols(sys);
		init_elemental_transmission_specs(sys);
		do_all_domains(sys, fstate);

		process_latency_queries(sys);

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


