#include <unordered_set>
#include "genie/genie.h"
#include "genie/net_topo.h"
#include "genie/net_rvd.h"
#include "genie/node_merge.h"
#include "genie/node_split.h"
#include "genie/lua/genie_lua.h"
#include "genie/graph.h"
#include "genie/value.h"
#include "genie/vlog.h"
#include "flow.h"
#include "net_rs.h"
#include "globals.h"
#include "node_flowconv.h"

using namespace genie;
using namespace genie::graphs;

namespace
{
	// Convert a network into a Graph
	typedef std::function<Port*(Port*)> N2GRemapFunc;
	Graph net_to_graph(System* sys, NetType ntype, 
		VAttr<Port*>* v_to_obj,
		RVAttr<Port*>* obj_to_v,
		EAttr<Link*>* e_to_link,
		REAttr<Link*>* link_to_e,
		const N2GRemapFunc& remap = N2GRemapFunc())
	{
		Graph result;

		// Gather all edges
		auto links = sys->get_links(ntype);

		// Whether or not the caller needs one, we require an obj->v map.
		// If the caller doesn't provide one, allocate (and then later free) a local one.
		bool local_map = !obj_to_v;
		if (local_map)
			obj_to_v = new RVAttr<Port*>;

		for (auto link : links)
		{
			// Get link's endpoints
			Port* src = link->get_src();
			Port* sink = link->get_sink();

			// Remap if a remap function was provided
			if (remap)
			{
				src = remap(src);
				sink = remap(sink);
			}

			// Create (or retrieve) associated vertex IDs for endpoints.
			// Update reverse association if caller requested one.
			VertexID src_v, sink_v;

			for (auto& p : {std::make_pair(&src_v, src), std::make_pair(&sink_v, sink)})
			{
				// src_v or sink_v
				VertexID* pv = p.first;
				// src or sink
				Port* obj = p.second;

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

	// Assign domains to RS Ports
	void rs_assign_domains(System* sys)
	{
		// Turn RS network into a graph.
		// Pass in a remap function which effectively treats all linkpoints as their parent ports.
		N2GRemapFunc remap = [=] (Port* o)
		{
			return RSPort::get_rs_port(o);
		};

		// We care about port->vid mapping and link->eid mapping
		REAttr<Link*> link_to_eid;
		RVAttr<Port*> port_to_vid;
		Graph rs_g = net_to_graph(sys, NET_RS, nullptr, &port_to_vid, nullptr, &link_to_eid, remap);

		// Identify connected components (domains).
		// Capture vid->domainid mapping and eid->domainid mapping
		VAttr<int> vid_to_domain;
		EAttr<int> eid_to_domain;
		connected_comp(rs_g, &vid_to_domain, &eid_to_domain);

		// Now assign each port its domain ID.
		for (auto& it : port_to_vid)
		{
			auto port = as_a<RSPort*>(it.first);
			assert(port);
			auto vid = it.second;
			
			// Look up the domain for this vid
			assert(vid_to_domain.count(vid));
			int domain = vid_to_domain[vid];

			port->set_domain_id(domain);
		}

		// Assign each RSLink its domain ID
		for (auto& it : link_to_eid)
		{
			auto link = as_a<RSLink*>(it.first);
			assert(link);
			auto eid = it.second;

			assert(eid_to_domain.count(eid));
			int domain = eid_to_domain[eid];

			link->set_domain_id(domain);
		}
	}

	// Assign Flow IDs to RS links
	void rs_assign_flow_ids(System* sys)
	{
		// Turn TOPO network into a graph. Merge together the input/output ports of split and merge
		// nodes for the purposes of graph construction.
		N2GRemapFunc remap = [] (Port* o) -> Port*
		{
			auto sp_node = as_a<NodeSplit*>(o->get_parent());
			if (sp_node) return sp_node->get_topo_input();
			
			auto mg_node = as_a<NodeMerge*>(o->get_parent());
			if (mg_node) return mg_node->get_topo_input();

			// Not an input/output port of a split/merge node. Do not remap.
			return o;
		};

		// Get TOPO network as graph. We need link->eid and HObj->vid
		EAttr<Link*> topo_eid_to_link;
		RVAttr<Port*> topo_port_to_vid;
		Graph topo_g = net_to_graph(sys, NET_TOPO, nullptr, &topo_port_to_vid, &topo_eid_to_link, nullptr,
			remap);

		// Split topo graph into connected domains
		EAttr<int> edge_to_comp;
		connected_comp(topo_g, nullptr, &edge_to_comp);

		auto comp_to_edges = util::reverse_map(edge_to_comp);

		for (auto& it : comp_to_edges)
		{
			// This is one domain

			// First, gather all the RS links carried by the TOPO links in this component.
			std::unordered_set<Link*> rs_links;
			for (auto& topo_edge : it.second)
			{
				Link* topo_link = topo_eid_to_link[topo_edge];
				auto ac = topo_link->asp_get<ALinkContainment>();
				auto parent_links = ac->get_all_parent_links(NET_RS);
				rs_links.insert(parent_links.begin(), parent_links.end());
			}

			// Next, gather all the sources of the RS links
			std::unordered_set<Endpoint*> rs_sources;
			for (Link* rs_link : rs_links)
			{
				auto src = rs_link->get_src_ep();
				assert(src);
				rs_sources.insert(src);
			}

			// Now start assigning Flow IDs to all the RS links emanating from each RS source.
			// IDs are assigned seqentially in a domain starting at 0.
			// All links coming from a single source (RSPort or RSLinkpoint) get the same ID
			Value flow_id = 0;
			for (auto rs_ep : rs_sources)
			{
				for (auto link : rs_ep->links())
				{
					auto rs_link = static_cast<RSLink*>(link);
					rs_link->set_flow_id(flow_id);
				}

				flow_id.set(flow_id + 1);
			}

			// Go back and calculate the width of all flow_ids in the domain, now that we know
			// the highest (final) value.
			int flow_id_width = std::max(1, util::log2(std::max(0, flow_id - 1)));
			for (auto link : rs_links)
			{
				auto rs_link = static_cast<RSLink*>(link);
				rs_link->get_flow_id().set_width(flow_id_width);
			}
		}

		/*
		topo_g.dump("flows", nullptr, [&](EdgeID eid)
		{
			// Topo domain.
			int domain = edge_to_comp[eid];
			std::string label = std::to_string(domain);

			label += ": ";

			// Append RS flow IDs
			auto link = topo_eid_to_link[eid];
			auto rs_links = link->asp_get<ALinkContainment>()->get_all_parent_links(NET_RS);
			for (auto& l : rs_links)
			{
				auto rsl = static_cast<RSLink*>(l);
				label += std::to_string(rsl->get_flow_id()) + ", ";
			}

			// Remove trailing comma
			label.pop_back();

			return label;
		});
		*/
	}


	void rs_refine_to_topo(System* sys)
	{
		// Create system contents ready for TOPO connectivity
		sys->refine(NET_TOPO);

		// Get the aspect that contains a reference to the System's Lua topology function
		auto atopo = sys->asp_get<ATopoFunc>();
		if (!atopo)
			throw HierException(sys, "no topology function defined for system");

		// Call topology function, passing the system as the first and only argument
		lua::push_ref(atopo->func_ref);
		lua::push_object(sys);
		lua::pcall_top(1, 0);

		// Unref topology function, and remove Aspect
		lua::free_ref(atopo->func_ref);
		sys->asp_remove<ATopoFunc>();
	}

	void topo_refine_to_rvd(System* sys)
	{
		// Refine all System contents such that they are RVD-connectable
		sys->refine(NET_RVD);

		// Go through all TOPO links in the system and create an RVD link for each.
		auto topo_links = sys->get_links(NET_TOPO);
		for (auto link : topo_links)
		{
			// These are the TOPO endpoints of the TOPO link
			auto src = as_a<TopoPort*>(link->get_src());
			auto sink = as_a<TopoPort*>(link->get_sink());
			assert(src);
			assert(sink);

			// We need to find corresponding RVD endpoints for the RVD link we're about to make.
			RVDPort* src_rvd = nullptr;
			RVDPort* sink_rvd = nullptr;

			// TOPO ports support multiple connections.
			// Each TOPO port contains an RVD port for each of those connections.
			// Here we find the first such unconnected RVD sub-port and use it as an endpoint
			// for our RVD link.
			for (int i = 0; i < src->get_n_rvd_ports(); i++)
			{
				src_rvd = src->get_rvd_port(i);
				if (!src_rvd->get_endpoint_sysface(NET_RVD)->is_connected())
					break;
			}

			for (int i = 0; i < sink->get_n_rvd_ports(); i++)
			{
				sink_rvd = sink->get_rvd_port(i);
				if (!sink_rvd->get_endpoint_sysface(NET_RVD)->is_connected())
					break;
			}

			// Connect RVD ports
			auto rvdlink = sys->connect(src_rvd, sink_rvd, NET_RVD);
			rvdlink->asp_get<ALinkContainment>()->add_parent_link(link);
		}
	}

	void rvd_insert_flow_convs(System* sys)
	{
		// Insert flow-converter nodes into the RVD network to convert lp_id<->flow_id.
		// This is done wherever an RVD source/sink has a RoleBinding for LP_ID.
		
		auto rvd_links = sys->get_links(NET_RVD);
		for (auto rvd_link : rvd_links)
		{
			auto src = as_a<RVDPort*>(rvd_link->get_src());
			auto sink = as_a<RVDPort*>(rvd_link->get_sink());

			for (auto port : {src, sink})
			{
				if (!port->has_role_binding(RVDPort::ROLE_DATA, "lpid"))
					continue;

				// Create flowconv node, name it after the port's hierarchical port name, relative
				// to the System.
				bool fc_to_flow = (port == src);
				std::string fc_name = hier_path_collapse(port->get_primary_port()->
					get_hier_path(sys)) + "_fc";
				auto fc_node = new NodeFlowConv(fc_to_flow);
				fc_node->set_name(fc_name);
				sys->add_child(fc_node);

				// Splice the node into the link between src/sink.
				// !!! make rvd_link point to the new fcnode->sink link in the case that
				// TWO fc_nodes are inserted on the original link. This relies on this for loop
				// visiting src first and sink second !!!
				rvd_link = sys->splice(rvd_link, fc_node->get_input(), fc_node->get_output());

				// Make the node set up its conversion tables, now that it's connected to stuff
				fc_node->configure();
			}
		}
	}

	void rvd_do_carriage(System* sys)
	{
		auto rs_links = sys->get_links(NET_RS);

		for (auto& link : rs_links)
		{

		}
	}

	// Do all work for one System
	void flow_do_system(System* sys)
	{
		// Process the RS links fresh after user's specification
		// Identify connected communication domains and assign a domain
		// index to each (physical, sans-linkpoint) RS port.
		rs_assign_domains(sys);

		// Create TOPO network from RS network
		rs_refine_to_topo(sys);

		// Assign Flow IDs to RS links and RS Ports based on topology and RS domain membership.
		// Must be done after TOPO network has been created.
		rs_assign_flow_ids(sys);

		// Create RVD network from TOPO network
		topo_refine_to_rvd(sys);
		
		// Various RVD processing
		rvd_insert_flow_convs(sys);
		rvd_do_carriage(sys);

		// Hand off to Verilog processing and output
		vlog::flow_process_system(sys);
	}
}


// Main processing entry point!
void flow_main()
{
	auto systems = genie::get_root()->get_systems();

	for (auto sys : systems)
	{
		flow_do_system(sys);

		if (Globals::inst()->dump_dot)
		{
			auto network = Network::get(Globals::inst()->dump_dot_network);
			if (!network)
				throw Exception("Unknown network " + Globals::inst()->dump_dot_network);

			sys->write_dot(sys->get_name() + "." + network->get_name(), network->get_id());
		}
	}
}

