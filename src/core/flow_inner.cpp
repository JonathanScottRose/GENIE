#include "pch.h"
#include "node_system.h"
#include "node_split.h"
#include "node_merge.h"
#include "node_user.h"
#include "node_conv.h"
#include "net_topo.h"
#include "net_rs.h"
#include "port_rs.h"
#include "flow.h"
#include "address.h"
#include "genie/port.h"

using namespace genie::impl;
using genie::Exception;
using Dir = genie::Port::Dir;

namespace
{
	void create_transmissions(NodeSystem* sys)
	{
		auto fstate = sys->get_flow_state_inner();

		// Go through all flows (logical RS links) in the domain.
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
				unsigned tid = fstate->new_transmission();

				for (auto link : addr_bin.second)
				{
					fstate->add_link_to_transmission(tid, link);
				}
			}
		}
	}

	void realize_topo_links(NodeSystem* sys)
	{
		auto link_rel = sys->get_link_relations();

		// Function that checks if a split or merge node belongs to a domain
		/*
		auto node_in_domain = [=](const Node* n)
		{
			auto tl = n->get_endpoint(NET_TOPO, Dir::IN)->get_link0();
			auto rl = link_rel->get_parents<LinkRSLogical>(tl, NET_RS_LOGICAL).front();
			return rl->get_domain_id() == dom_id;
		};
		*/

		// Gather splits and merges for this domain
		auto splits = sys->get_children_by_type<NodeSplit>();
		auto merges = sys->get_children_by_type<NodeMerge>();

		// Tell splits and merges to create their ports
		for (auto sp : splits)
		{
			sp->create_ports();
		}

		for (auto mg : merges)
		{
			mg->create_ports();
		}

		// Go over all topological links and realize into physical RS links.
		// Also, associate these new physical links with the topo links
		for (auto topo_link : sys->get_links(NET_TOPO))
		{
			PortRS* rs_src = nullptr;
			PortRS* rs_sink = nullptr;

			HierObject* topo_src = topo_link->get_src();
			HierObject* topo_sink = topo_link->get_sink();

			//
			// Source
			//

			if (auto sp = dynamic_cast<NodeSplit*>(topo_src))
			{
				// Choose an unnused output
				unsigned n_out = sp->get_n_outputs();
				for (unsigned i = 0; i < n_out; i++)
				{
					PortRS* output = sp->get_output(i);
					if (!output->get_endpoint(NET_RS_PHYS, Dir::OUT)->is_connected())
					{
						rs_src = output;
						break;
					}
				}
			}
			else if (auto mg = dynamic_cast<NodeMerge*>(topo_src))
			{
				rs_src = mg->get_output();
			}
			else if (auto rs = dynamic_cast<PortRS*>(topo_src))
			{
				rs_src = rs;
			}
			else
			{
				assert(false);
			}

			//
			// Sink
			//
			
			if (auto sp = dynamic_cast<NodeSplit*>(topo_sink))
			{
				rs_sink = sp->get_input();
			}
			else if (auto mg = dynamic_cast<NodeMerge*>(topo_sink))
			{
				// Choose an unnused input
				unsigned n_in = mg->get_n_inputs();
				for (unsigned i = 0; i < n_in; i++)
				{
					PortRS* input = mg->get_input(i);
					if (!input->get_endpoint(NET_RS_PHYS, Dir::IN)->is_connected())
					{
						rs_sink = input;
						break;
					}
				}
			}
			else if (auto rs = dynamic_cast<PortRS*>(topo_sink))
			{
				rs_sink = rs;
			}
			else
			{
				assert(false);
			}

			// Connect
			auto rs_link = static_cast<LinkRSPhys*>(sys->connect(rs_src, rs_sink, NET_RS_PHYS));

			// Associate
			link_rel->add(topo_link, rs_link);
		}
	}

	void insert_addr_converters_user(NodeSystem* sys)
	{
		auto fstate = sys->get_flow_state_inner();
		auto& glob_rep = fstate->get_flow_rep();

		// First, handle user addr <-> global rep conversions.
		// Gather all RS ports from: this system, user modules
		std::vector<Node*> gather_nodes = 
			sys->get_children_by_type<NodeUser, decltype(gather_nodes)>();
		gather_nodes.push_back(sys);

		std::vector<PortRS*> user_ports;
		for (auto node : gather_nodes)
		{
			auto ports = node->get_children_by_type<PortRS>();
			user_ports.insert(user_ports.end(), ports.begin(), ports.end());
		}

		// For every user port (that has a NET_RS_PHYS link), check for useraddr fields,
		// and insert converters
		for (auto user_port : user_ports)
		{
			// System-facing endpoint
			auto ep = user_port->get_endpoint(NET_RS_PHYS, user_port->get_effective_dir(sys));
			auto rs_link = static_cast<LinkRSPhys*>(ep->get_link0());
			if (!rs_link)
				continue;

			auto& proto = user_port->get_proto();
			if (proto.has_terminal_field(FIELD_USERADDR))
			{
				// Derive user address representation.
				// If only one address bin exists, no conversion is necessary,
				// as that one bin's value can be injected as a constant.
				auto user_rep = flow::make_srcsink_flow_rep(sys, user_port);
				if (user_rep.get_n_addr_bins() <= 1)
					continue;

				// Create a converter node.
				auto conv = new NodeConv();

				// Make a name for it
				std::string conv_name = util::str_con_cat("conv", user_port->get_hier_path(sys));
				std::replace(conv_name.begin(), conv_name.end(), HierObject::PATH_SEP, '_');
				conv->set_name(conv_name);

				// Insert node, splice into link
				sys->add_child(conv);
				sys->splice(rs_link, conv->get_input(), conv->get_output());

				// Find out the direction of conversion and configure the converter
				bool to_user = ep->get_dir() == Port::Dir::IN;
				auto& in_rep = to_user ? glob_rep : user_rep;
				auto& out_rep = to_user ? user_rep : glob_rep;
				auto in_field = to_user ? FIELD_USERADDR : FIELD_XMIS_ID;
				auto out_field = to_user ? FIELD_XMIS_ID : FIELD_USERADDR;

				conv->configure(in_rep, in_field, out_rep, out_field);
			}
		}
	}

	void insert_addr_converters_split(NodeSystem* sys)
	{
		auto fstate = sys->get_flow_state_inner();
		auto& glob_rep = fstate->get_flow_rep();

		// Go through all split nodes
		for (auto sp : sys->get_children_by_type<NodeSplit>())
		{
			// Make an addr representation for its input
			auto sp_rep = flow::make_split_node_rep(sys, sp);

			// If there's just one address bin, then the split node always
			// broadcasts to the same outputs. We can tie off the mask with
			// a constant, rather than inserting a converter.
			if (sp_rep.get_n_addr_bins() == 1)
			{
				auto& proto = sp->get_input()->get_proto();
				unsigned addr = sp_rep.get_addr_bins().begin()->first;
				unsigned n_bits = sp->get_n_outputs();
				
				BitsVal conzt(n_bits, 1);
				conzt.set_val(0, 0, addr, n_bits);
				
				proto.set_const(FIELD_SPLITMASK, conzt);
			}
			else
			{
				// Make a conv node
				std::string conv_name = util::str_con_cat("conv", sp->get_hier_path(sys));
				std::replace(conv_name.begin(), conv_name.end(), HierObject::PATH_SEP, '_');

				auto conv = new NodeConv();
				conv->set_name(conv_name);

				// Split input and link
				auto sp_in = sp->get_input();
				auto sp_link = sp_in->get_endpoint(NET_RS_PHYS, Port::Dir::IN)->get_link0();

				// Add and splice in the conv node
				sys->add_child(conv);
				sys->splice(sp_link, conv->get_input(), conv->get_output());

				// Configure conv node.
				// Convert global representation to split node one-hot mask
				conv->configure(glob_rep, FIELD_XMIS_ID, sp_rep, FIELD_SPLITMASK);
			}
		}
	}

	void do_protocol_carriage(NodeSystem* sys)
	{
		auto e2e_links = sys->get_links(NET_RS_LOGICAL);
		auto link_rel = sys->get_link_relations();

		// Traverse every end-to-end elemental transmission
		for (auto& e2e_link : e2e_links)
		{
			auto e2e_src = static_cast<PortRS*>(e2e_link->get_src());
			auto e2e_sink = static_cast<PortRS*>(e2e_link->get_sink());

			// Initialize a carriage set to empty. This holds the Fields that need to be
			// carried across the next physical link 
			FieldSet carriage_set;

			// Start at the final sink and work backwards to the source
			auto cur_sink = e2e_sink;

			// Loop until we've traversed the entire chain of links from sink to source
			while (true)
			{
				// First, get the link feeding cur_sink, along with the feeder port
				auto ext_link = cur_sink->get_endpoint(NET_RS_PHYS, Port::Dir::IN)->get_link0();
				assert(ext_link);
				auto cur_src = static_cast<PortRS*>(ext_link->get_src());

				// Exit if we've reached the beginning
				if (cur_src == e2e_src)
					break;

				// Protocols of local src/sink
				auto& sink_proto = cur_sink->get_proto();
				auto& src_proto = cur_src->get_proto();

				// Carriage set += (what sink needs) - (what src provides)
				carriage_set.add(sink_proto.terminal_fields());
				carriage_set.subtract(src_proto.terminal_fields());

				// Make the intermediate Node carry the carriage set, if it is able.
				// If not, reset the carriage set.
				CarrierProtocol* carrier_proto = cur_src->get_carried_proto();
				if (carrier_proto)
					carrier_proto->add_set(carriage_set);
				else
					carriage_set.clear();

				// Traverse backwards internally through the Node to find the next 
				// input port to visit.
				// This is relevant
				cur_sink = nullptr;
				auto cur_src_ep = cur_src->get_endpoint(NET_RS_PHYS, Port::Dir::IN);
				for (auto int_link : cur_src_ep->links())
				{
					// Candidate sink that feeds cur_src through the node.
					auto cand_sink = static_cast<PortRS*>(int_link->get_src());

					// Get the physical links feeding it
					auto cand_feeder =
						cand_sink->get_endpoint(NET_RS_PHYS, Port::Dir::IN)->get_link0();

					// If this physical link carries the end-to-end transmission we want,
					// then choose it to continue tranversal.
					if (link_rel->is_contained_in(e2e_link, cand_feeder))
					{
						cur_sink = cand_sink;
						break;
					}
				}

				// If this fails, then traversal doesn't know where to go next.
				assert(cur_sink);
			}
		}
	}

	void do_backpressure(NodeSystem* sys)
	{
		using namespace graph;
		auto phys_links = sys->get_links(NET_RS_PHYS);

		Attr2V<HierObject*> port_to_v;
		V2Attr<HierObject*> v_to_port;
		Graph g = flow::net_to_graph(sys, NET_RS_PHYS, true, 
			&v_to_port, &port_to_v, nullptr, nullptr);

		// Populate initial visitation list with ports that are either:
		// 1) terminal (no outgoing edges)
		// 2) Have a known/forced backpressure setting
		// Then do a depth-first reverse traversal, visiting and updating all feeders that are
		// configurable.
		std::deque<VertexID> to_visit;

		for (auto v : g.iter_verts)
		{
			bool add = g.dir_neigh(v).empty();
			add |= ((PortRS*)(v_to_port[v]))->get_bp_status().configurable == false;

			if (add)
				to_visit.push_back(v);
		}

		while (!to_visit.empty())
		{
			VertexID cur_v = to_visit.back();
			to_visit.pop_back();

			auto cur_port = (PortRS*)v_to_port[cur_v];
			auto& cur_bp = cur_port->get_bp_status();

			if (cur_bp.configurable && cur_bp.status == RSBackpressure::UNSET)
			{
				// Given the choice, we'd like to not have this.
				cur_bp.status = RSBackpressure::DISABLED;
			}
			else if (!cur_bp.configurable)
			{
				// If it's unchangeable, make sure it's set to something
				assert(cur_bp.status != RSBackpressure::UNSET);
			}

			// Get feeders of this port. Could be internal or external links.
			auto feeders = g.dir_neigh_r(cur_v);

			for (auto next_v : feeders)
			{
				auto next_port = (PortRS*)v_to_port[next_v];
				auto& next_bp = next_port->get_bp_status();

				// Is the feeder configurable?
				if (next_bp.configurable)
				{
					// If it's unset, updated it to match.
					// If it's set to DISABLED and the incoming is ENABLED, update it to match too.
					// In both cases, put the destination on the visitation stack to continue the update.
					//
					// Else (it's set and: matches incoming, or is ENABLED and incoming is DISABLED),
					// do nothing.

					if (next_bp.status == RSBackpressure::UNSET ||
						next_bp.status == RSBackpressure::DISABLED &&
						cur_bp.status == RSBackpressure::ENABLED)
					{
						next_bp.status = cur_bp.status;
						to_visit.push_back(next_v);
					}
				}
				else
				{
					// Not configurable? Just make sure backpressures are compatible then.
					assert(next_bp.status != RSBackpressure::UNSET);

					if (next_bp.status == RSBackpressure::DISABLED &&
						cur_bp.status == RSBackpressure::ENABLED)
					{
						throw Exception("Incompatible backpressure: " + cur_port->get_hier_path() +
							" provides but " + next_port->get_hier_path() + " does not consume");
					}
				}
			} // foreach feeder
		}
	}

	void default_eops(NodeSystem* sys)
	{
		// Default unconnected EOPs to 1
		auto links = sys->get_links(NET_RS_PHYS);
		for (auto link : links)
		{
			auto src = (PortRS*)link->get_src();
			auto sink = (PortRS*)link->get_sink();

			auto& src_proto = src->get_proto();
			auto& sink_proto = sink->get_proto();

			if (!src->has_field(FIELD_EOP) && sink->has_field(FIELD_EOP))
			{
				sink_proto.set_const(FIELD_EOP, BitsVal(1).set_bit(0, 1));
			}
		}
	}

	void default_addr_reps(NodeSystem* sys)
	{

	}
}

void flow::do_inner(NodeSystem* sys)
{
	sys->set_flow_state_inner(new FlowStateInner);

	create_transmissions(sys);
	flow::make_internal_flow_rep(sys);

	realize_topo_links(sys);
	insert_addr_converters_user(sys);
	insert_addr_converters_split(sys);
	do_protocol_carriage(sys);

	do_backpressure(sys);
	default_eops(sys);
	default_addr_reps(sys);
}