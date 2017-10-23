#include "pch.h"
#include "node_system.h"
#include "node_split.h"
#include "node_merge.h"
#include "node_user.h"
#include "node_conv.h"
#include "node_clockx.h"
#include "node_reg.h"
#include "node_mdelay.h"
#include "net_topo.h"
#include "net_rs.h"
#include "net_clockreset.h"
#include "port_rs.h"
#include "port_clockreset.h"
#include "flow.h"
#include "address.h"
#include "genie/port.h"

using namespace genie::impl;
using genie::Exception;
using Dir = genie::Port::Dir;
using flow::FlowStateOuter;

namespace
{
	class FlowStateInner
	{
	public:
		NodeSystem* sys = nullptr;
		FlowStateOuter* outer = nullptr;
		unsigned dom_id;
		AddressRep domain_addr_rep;
	};


	void make_domain_addr_rep(FlowStateInner& fs_in)
	{
		auto fs_out = fs_in.outer;
		auto& rep = fs_in.domain_addr_rep;
		auto dom = fs_out->get_rs_domain(fs_in.dom_id);
		auto& dom_xmis = dom->get_transmissions();

		// Take each transmission in one domain, and give it a unique ID
		unsigned n_xmis = dom_xmis.size();
		for (unsigned i = 0; i < n_xmis; i++)
		{
			unsigned xmis_id = dom_xmis[i];
			rep.insert(xmis_id, i);
		}
	}

	AddressRep make_split_node_rep(FlowStateInner& fstate, NodeSplit* sp)
	{
		AddressRep result;

		auto sys = fstate.sys;
		auto fs_out = fstate.outer;
		auto& link_rel = sys->get_link_relations();

		// transmission ID to address.
		std::unordered_map<unsigned, unsigned> trans2addr;

		// Find out which outputs each transmission's flows go to, and create
		// a one-hot mask out of that.
		unsigned n_out = sp->get_n_outputs();
		for (unsigned i = 0; i < n_out; i++)
		{
			auto port = sp->get_output(i);
			auto out_link = port->get_endpoint(NET_RS_PHYS, Port::Dir::OUT)->get_link0();

			// Get logical links from physical link
			auto rs_links = link_rel.get_parents(out_link->get_id(), NET_RS_LOGICAL);

			for (auto rs_link : rs_links)
			{
				// Find the transmission and its ID for each rs link.
				// Add to bitmask
				auto xmis_id = fs_out->get_transmission_for_link(rs_link);

				trans2addr[xmis_id] |= (1 << i);
			}
		}

		// Copy address assignments to result
		for (auto it : trans2addr)
		{
			result.insert(it.first, it.second);
		}

		return result;
	}

	AddressRep make_srcsink_flow_rep(FlowStateInner& fstate, PortRS* srcsink)
	{
		AddressRep result;

		auto sys = fstate.sys;
		auto fs_out = fstate.outer;

		// Find out whether we're dealing with a source or sink.
		auto dir = srcsink->get_effective_dir(sys);

		// Get the appropriate endpoint
		auto ep = srcsink->get_endpoint(NET_RS_LOGICAL, dir);

		// Get links
		auto& links = ep->links();

		// Bin by src or sink address
		for (auto link : links)
		{
			auto rs_link = static_cast<LinkRSLogical*>(link);
			auto xmis_id = fs_out->get_transmission_for_link(rs_link->get_id());

			auto user_addr = dir == Port::Dir::OUT ?
				rs_link->get_src_addr() : rs_link->get_sink_addr();

			result.insert(xmis_id, user_addr);
		}

		return result;
	}

	void check_merge_node_exclusivity(FlowStateInner& fstate, NodeMerge* mg)
	{
		// Merge node is exclusive if:
		// for any two incoming topo links (aka merge node inputs), every
		// logical link carried by the first topo link is marked temporally mutually
		// exclusive with every logical link carried by the second topo link.
		
		auto sys = fstate.sys;
		auto& rel = sys->get_link_relations();
		auto fs_out = fstate.outer;

		auto& tlinks = mg->get_endpoint(NET_TOPO, Dir::IN)->links();
		bool exclusive = true; // assume until proven otherwise

		for (auto it_topo1 = tlinks.begin(); 
			exclusive && it_topo1 != tlinks.end(); ++it_topo1)
		{
			auto topo1 = *it_topo1;
			auto logicals1 = rel.get_parents(topo1->get_id(), NET_RS_LOGICAL);
			
			for (auto it_topo2 = it_topo1 + 1; 
				exclusive && it_topo2 != tlinks.end(); ++it_topo2)
			{
				auto topo2 = *it_topo2;
				auto logicals2 = rel.get_parents(topo2->get_id(), NET_RS_LOGICAL);

				// Now check logicals against each other. Two things to check:
				// 1) do they belong to the same transmission? if so, exclusive.
				// 2) are they marked explicitly exclusive by the user? 
				for (auto it_log1 = logicals1.begin(); 
					exclusive && it_log1 != logicals1.end(); ++it_log1)
				{
					auto log1 = *it_log1;
					auto xmis1 = fs_out->get_transmission_for_link(log1);
					for (auto it_log2 = logicals2.begin(); 
						exclusive && it_log2 != logicals2.end(); ++it_log2)
					{
						auto log2 = *it_log2;
						auto xmis2 = fs_out->get_transmission_for_link(log2);
						if (xmis1 != xmis2 && !fstate.outer->are_transmissions_exclusive(xmis1, xmis2))
						{
							exclusive = false;
							break;
						}
					}
				}
			}
		}

		mg->set_exclusive(exclusive);
	}

	void realize_topo_links(FlowStateInner& fstate)
	{
		auto sys = fstate.sys;
		auto& link_rel = sys->get_link_relations();

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
			check_merge_node_exclusivity(fstate, mg);
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
			link_rel.add(topo_link->get_id(), rs_link->get_id());
		}
	}

	void insert_addr_converters_user(FlowStateInner& fstate)
	{
		auto sys = fstate.sys;
		auto& glob_rep = fstate.domain_addr_rep;

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
			auto user_port_dir = user_port->get_effective_dir(sys);
			auto ep = user_port->get_endpoint(NET_RS_PHYS, user_port_dir);
			auto rs_link = static_cast<LinkRSPhys*>(ep->get_link0());
			if (!rs_link)
				continue;
			
			auto& proto = user_port->get_proto();
			if (proto.has_terminal_field(FIELD_USERADDR))
			{
				// Derive user address representation.
				// If only one address bin exists, no conversion is necessary,
				// as that one bin's value can be injected as a constant.
				auto user_rep = make_srcsink_flow_rep(fstate, user_port);

				auto& addr_bins = user_rep.get_addr_bins();
				if (addr_bins.size() == 1)
				{
					// Just one bin. We can const it!
					unsigned addr = addr_bins.begin()->first;
					unsigned addr_bits = std::max(1U, user_rep.get_size_in_bits());

					// If this addr is ADDR_ANY, then the user did a silly thing:
					// they gave this port an address signal but didn't make use of it.
					// Since they don't care, we'll pick an const 0.

					if (addr == AddressRep::ADDR_ANY)
					{
						genie::log::warn("%s: has address signal but no bound transmissions.",
							user_port->get_hier_path().c_str());
						addr = 0;
					}

					// Do this for sink ports only
					if (user_port_dir == Port::Dir::IN)
					{
						proto.set_const(FIELD_USERADDR, 
							BitsVal(addr_bits).set_val(0, 0, addr, addr_bits));
					}
										
					continue;
				}
				else if (user_rep.exists(AddressRep::ADDR_ANY))
				{
					// If there's more than one address bin, none of them must be ADDR_ANY
					
					throw Exception(user_port->get_hier_path() + 
						": not all transmissions are bound to an address");
				}

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
				auto in_field = to_user ? FIELD_XMIS_ID : FIELD_USERADDR;
				auto out_field = to_user ? FIELD_USERADDR : FIELD_XMIS_ID;

				conv->configure(in_rep, in_field, out_rep, out_field);
			}
		}
	}

	void insert_addr_converters_split(FlowStateInner& fstate)
	{
		auto sys = fstate.sys;
		auto& glob_rep = fstate.domain_addr_rep;

		// Go through all split nodes
		for (auto sp : sys->get_children_by_type<NodeSplit>())
		{
			// Make an addr representation for its input
			auto sp_rep = make_split_node_rep(fstate, sp);

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

	void do_protocol_carriage(FlowStateInner& fstate)
	{
		auto sys = fstate.sys;

		auto e2e_links = sys->get_links(NET_RS_LOGICAL);
		auto& link_rel = sys->get_link_relations();

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
				carriage_set.add(sink_proto.terminal_fields_nonconst());
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
					if (link_rel.is_contained_in(e2e_link->get_id(), cand_feeder->get_id()))
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

	void do_backpressure(FlowStateInner& fstate)
	{
		using namespace graph;
		auto sys = fstate.sys;
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

		std::copy_if(g.iter_verts.begin(), g.iter_verts.end(), std::back_inserter(to_visit),
			[&](VertexID v)
		{
			bool is_terminal = g.dir_neigh(v).empty();
			bool bp_known = ((PortRS*)(v_to_port[v]))->get_bp_status().configurable == false;
			return is_terminal || bp_known;
		});

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
					// If it's unset, update it to match.
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

	void splice_backpressure(PortRS* orig_src, PortRS* new_sink, PortRS* new_src, PortRS* orig_sink)
	{
		auto& orig_sink_bp = orig_sink->get_bp_status();
		RSBackpressure::Status cur_status = orig_sink_bp.status;
		
		// Make sure orig_sink has been decided already.
		// We will propagate this backwards.
		assert(cur_status != RSBackpressure::UNSET);

		// Propagate through new_src and new_sink
		for (auto port : { new_src, new_sink })
		{
			auto& bp = port->get_bp_status();

			if (bp.configurable)
			{
				// If it's disabled, keep it the same or upgrade it to enabled.
				// If it's enabled, keep it that way and propagate the enabledness further back
				if (bp.status == RSBackpressure::DISABLED ||
					bp.status == RSBackpressure::UNSET)
					bp.status = cur_status;
				else
					cur_status = RSBackpressure::ENABLED;
			}
			else
			{
				// If not configurable, make sure it's not mismatched in a bad way
				assert(!(cur_status == RSBackpressure::ENABLED &&
					bp.status == RSBackpressure::DISABLED));
			}
		}

		// Make sure orig_src is the same setting
		auto& orig_src_bp = orig_src->get_bp_status();
		assert(orig_src_bp.status == cur_status);
	}

	void default_eops(FlowStateInner& fstate)
	{
		// Default unconnected EOPs to 1
		auto links = fstate.sys->get_links(NET_RS_PHYS);
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

	void default_xmis_ids(FlowStateInner& fstate_in)
	{
		auto sys = fstate_in.sys;
		auto fstate_out = fstate_in.outer;
		auto& link_rel = sys->get_link_relations();
		auto& addr_rep = fstate_in.domain_addr_rep;

		// Gather all physical RS links where:
		// - the sink needs an xmis_id field
		// - the source doesn't have one.

		auto phys_links = sys->get_links(NET_RS_PHYS);
		for (auto phys_link : phys_links)
		{
			auto src = static_cast<PortRS*>(phys_link->get_src());
			auto sink = static_cast<PortRS*>(phys_link->get_sink());

			if (sink->has_field(FIELD_XMIS_ID) && !src->has_field(FIELD_XMIS_ID))
			{
				// Now, gather all the logical RS links passing through this physical link.
				// All these locial RS links MUST belong to the same transmission ID,
				// otherwise a converter failed to be inserted earlier in the flow.
				//
				// Once we have this single transmission's ID, we can insert it as a constant
				// into the sink's port protocol.

				auto log_link_ids = link_rel.get_parents(phys_link->get_id(), NET_RS_LOGICAL);
				unsigned xmis_id = AddressRep::ADDR_INVALID;

				for (auto log_link_id : log_link_ids)
				{
					auto this_xmis_id = fstate_out->get_transmission_for_link(log_link_id);

					// Make sure all logical links have the same transmission ID
					if (xmis_id != AddressRep::ADDR_INVALID && xmis_id != this_xmis_id)
					{
						assert(false);
					}
					else
					{
						xmis_id = this_xmis_id;
					}
				}

				// Convert transmission ID into address
				unsigned xmis_addr = addr_rep.get_addr(xmis_id);
				
				FieldInst* field = sink->get_field(FIELD_XMIS_ID);
				BitsVal xmis_id_val(field->get_width());
				xmis_id_val.set_val(0, 0, xmis_addr, field->get_width());

				auto& sink_proto = sink->get_proto();
				sink_proto.set_const(FIELD_XMIS_ID, xmis_id_val);
			}
		}
	}

	void connect_resets(FlowStateInner& fstate)
	{
		auto sys = fstate.sys;
		// Finds dangling reset inputs and connects them to either:
		// 1) Any existing reset input in the system
		// 2) A freshly-created reset input, if 1) didn't find one

		// Find any unbound reset sinks on any nodes
		std::vector<PortReset*> sinks_needing_connection;

		auto nodes = sys->get_nodes();
		for (auto node : nodes)
		{
			auto reset_sinks = node->get_children_by_type<PortReset>();
			for (auto reset_sink : reset_sinks)
			{
				// Sinks only
				if (reset_sink->get_dir() != Dir::IN)
					continue;

				// Unconnected sinks only.
				if (reset_sink->get_endpoint(NET_RESET, Dir::IN)->is_connected())
					continue;

				// Add to list
				sinks_needing_connection.push_back(reset_sink);
			}
		}

		// Exit early if we have no work to do
		if (sinks_needing_connection.empty())
			return;

		// Find or create the reset source.
		PortReset* reset_src = nullptr;
		auto reset_srces = sys->get_children<PortReset>([](const HierObject* o)
		{
			auto oo = dynamic_cast<const PortReset*>(o);
			return oo && oo->get_dir() == Dir::IN;
		});

		if (!reset_srces.empty())
		{
			// Found an existing one. Any will do right now!
			reset_src = reset_srces.front();
		}
		else
		{
			throw Exception(sys->get_hier_path() + ": needs at least one reset port");
			/*
			std::string auto_reset_port_name = "reset_autogen";

			// Create one.
			genie::log::warn("%s: no system reset port, automatically creating '%s'",
				sys->get_name().c_str(), auto_reset_port_name.c_str());

			reset_src = dynamic_cast<PortReset*>
				(sys->create_reset_port(auto_reset_port_name, Dir::IN, auto_reset_port_name));
				*/
		}

		// Connect.
		for (auto reset_sink : sinks_needing_connection)
		{
			sys->connect(reset_src, reset_sink, NET_RESET);
		}
	}

	void connect_clocks(FlowStateInner& fstate)
	{
		using namespace graph;
		auto sys = fstate.sys;

		// The system contains interconnect-related nodes which have no clocks connected yet.
		// When there are multiple clock domains, the choice of which clock domain to assign
		// which interconnect node is an optimization problem: we want to minimize the total number
		// of data bits that need clock crossing. This is an instance of a Multiway Graph Cut problem, 
		// which is optimally solvable in polynomial time for k=2 clocks, but is NP-hard for 3 or more.
		// We will use a greedy heuristic.

		// The implementation of the algorithm sits in a different file, and works on a generic graph.
		// Here, we need to generate this graph so that we can properly express the problem at hand.
		// In the generated graph, each vertex will correspond to a ClockResetPort of some Node, which 
		// either already has a clock assignment (making it a 'terminal' in the multiway problem) or
		// has yet to have a clock assigned, which is ultimately what we want to find out.

		// So, a Vertex represents a clock input port. An edge will represent a Conn between two DataPorts, 
		// and the two vertices of the edge are the clock input ports associated with the DataPorts. Each edge
		// will be weighted with the total number of PhysField bits in the protocol. In the case that the protocols
		// are different between the two DataPorts, we will take the union (only the physfields present in both) since
		// the missing ones will be defaulted later.

		// Graph to do multiway algorithm on
		Graph G;

		// List of terminal vertices (one for each clock domain)
		VList T;

		// Edge weights in G
		E2Attr<int> weights;

		// Maps UNCONNECTED clock sinks to vertex IDs in G
		Attr2E<PortClock*> clocksink_to_vid;
		V2Attr<PortClock*> vid_to_clocksink;

		// Maps clock sources (aka clock domains) to vertex IDs in G, and vice versa
		Attr2V<PortClock*> clocksrc_to_vid;
		V2Attr<PortClock*> vid_to_clocksrc;

		// Construct G and the inputs to MWC algorithm
		auto phys_links = sys->get_links(NET_RS_PHYS);
		for (auto phys_link : phys_links)
		{
			auto phys_a = (PortRS*)phys_link->get_src();
			auto phys_b = (PortRS*)phys_link->get_sink();

			// Get the clock sinks associated with each port
			auto csink_a = phys_a->get_clock_port();
			auto csink_b = phys_b->get_clock_port();

			assert(csink_a && csink_b);

			// Two RS ports in same node connected to each other, under same clock domain.
			// This creates self-loops in the graph and derps up the MWC algorithm.
			if (csink_a == csink_b)
				continue;

			// Get the clock ports driving each clock sink
			auto csrc_a = csink_a->get_connected_clk_port(sys);
			auto csrc_b = csink_b->get_connected_clk_port(sys);

			// Vertices representing the two clock sinks. A single vertex is created for:
			// 1) Each undriven clock sink
			// 2) All clock sinks driven by the same clock source
			VertexID v_a, v_b;

			// Handle the _a and _b identically with this loop, to avoid code duplication
			for (auto& i : { std::forward_as_tuple(v_a, csrc_a, csink_a),
				std::forward_as_tuple(v_b, csrc_b, csink_b) })
			{
				auto& v = std::get<0>(i);
				auto csrc = std::get<1>(i);
				auto csink = std::get<2>(i);

				if (csrc)
				{
					// Case 1) Clock sink is driven. Create or get the vertex associated with
					// the DRIVER of the clock sink.
					if (clocksrc_to_vid.count(csrc))
					{
						v = clocksrc_to_vid[csrc];
					}
					else
					{
						v = G.newv();
						clocksrc_to_vid[csrc] = v;
						vid_to_clocksrc[v] = csrc;

						// Also add this new vertex to our list of Terminal vertices
						T.push_back(v);
					}
				}
				else
				{
					// Case 2) Clock sink is UNdriven. Create or get the vertex associated with
					// the sink itself.
					if (clocksink_to_vid.count(csink))
					{
						v = clocksink_to_vid[csink];
					}
					else
					{
						v = G.newv();
						clocksink_to_vid[csink] = v;
						vid_to_clocksink[v] = csink;
					}
				}
			}

			// Avoid self-loops
			if (v_a == v_b)
				continue;

			// The weight of the edge is the width of signals crossing from a to b
			int weight = (int)flow::calc_transmitted_width(phys_a, phys_b);

			// Eliminiate 0 weights
			weight++;

			// Create and add edge with weight
			EdgeID e = G.newe(v_a, v_b);
			weights[e] = weight;
		}

		/*
		sys->write_dot("rvd_premwc", NET_RVD);
		G.dump("mwc", [&](VertexID v)
		{
		ClockPort* p = nullptr;
		if (vid_to_clocksink.count(v)) p = vid_to_clocksink[v];
		if (vid_to_clocksrc.count(v))
		{
		assert(!p);
		p = vid_to_clocksrc[v];
		}

		return p->get_hier_path(sys) + " (" +  std::to_string(v) + ")";
		}, [&](EdgeID e)
		{
		return std::to_string(weights[e]);
		});*/

		// Call multiway cut algorithm, get vertex->terminal mapping
		auto vid_to_terminal = graph::multi_way_cut(G, weights, T);

		// Iterate over all unconnected clock sinks, and connect them to the clock source
		// identified by the output of the MWC algorithm.
		for (auto& i : clocksink_to_vid)
		{
			PortClock* csink = i.first;
			VertexID v_sink = i.second;

			VertexID v_src = vid_to_terminal[v_sink];
			PortClock* csrc = vid_to_clocksrc[v_src];

			sys->connect(csrc, csink, NET_CLOCK);
		}
	}

	void insert_clockx(FlowStateInner& fstate)
	{
		auto sys = fstate.sys;

		// Insert clock crossing adapters on clock domain boundaries.
		// A clock domain boundary exists on any phys link where the source/sink clock drivers differ.
		unsigned nodenum = 0;
		auto links = sys->get_links(NET_RS_PHYS);

		for (auto orig_link : links)
		{
			// source/sink ports
			auto port_a = (PortRS*)orig_link->get_src();
			auto port_b = (PortRS*)orig_link->get_sink();

			// Clock sinks of each RVD port
			auto csink_a = port_a->get_clock_port();
			auto csink_b = port_b->get_clock_port();

			// Clock drivers of clock sinks of each port
			auto csrc_a = csink_a->get_driver(sys);
			auto csrc_b = csink_b->get_driver(sys);

			assert(csrc_a && csrc_b);

			// We only care about mismatched domains
			if (csrc_a == csrc_b)
				continue;

			// Create and add clockx node, and give a name that's unique
			// even across domains
			auto cxnode = new NodeClockX();
			cxnode->set_name(util::str_con_cat("clockx",
				std::to_string(fstate.dom_id),
				std::to_string(nodenum++)));
			sys->add_child(cxnode);

			// Connect its clock inputs
			sys->connect(csrc_a, cxnode->get_inclock_port(), NET_CLOCK);
			sys->connect(csrc_b, cxnode->get_outclock_port(), NET_CLOCK);

			// Splice the node into the existing connection
			sys->splice(orig_link, cxnode->get_indata_port(), cxnode->get_outdata_port());
			flow::splice_carrier_protocol(port_a, port_b, cxnode);
		}
	}

	void realize_latencies(FlowStateInner& fstate)
	{
		auto sys = fstate.sys;
		// Find phys links with nonzero latency.
		// Insert reg nodes or mdelay nodes.
		// Reset latencies to zero.

		auto phys_links = sys->get_links_casted<LinkRSPhys>(NET_RS_PHYS);
		std::vector<LinkRSPhys*> links_to_process;
		for (auto link : phys_links)
		{
			if (link->get_latency() > 0)
				links_to_process.push_back(link);
		}

		// Used to make unique names
		unsigned pipe_no = 0;

		for (auto orig_link : links_to_process)
		{
			// Get the clock domain of the source (arbitrarily, could have done sink too)
			PortClock* clock_driver = nullptr;
			{
				auto orig_src = (PortRS*)orig_link->get_src();
				PortClock* clock_sink = orig_src->get_clock_port();
				clock_driver = clock_sink->get_driver(sys);
			}
			assert(clock_driver);

			unsigned width = flow::calc_transmitted_width(orig_link);
			unsigned latency = orig_link->get_latency();

			// Insert a chain of regs, or a mem, depending on which is cheaper
			unsigned reg_cost = width * latency;
			unsigned mem_cost = width + latency + 8; // TODO: is the 8 hardcoded?

			if (!genie::impl::get_flow_options().no_mdelay && mem_cost < reg_cost)
			{
				auto md = new NodeMDelay();
				md->set_delay(latency);
				md->set_name(util::str_con_cat("pipe", std::to_string(fstate.dom_id),
					std::to_string(pipe_no)));
				sys->add_child(md);

				auto orig_src = (PortRS*)orig_link->get_src();
				auto orig_sink = (PortRS*)orig_link->get_sink();

				sys->splice(orig_link, md->get_input(), md->get_output());
				sys->connect(clock_driver, md->get_clock_port(), NET_CLOCK);
				flow::splice_carrier_protocol(orig_src, orig_sink, md);
				splice_backpressure(orig_src, md->get_input(), md->get_output(), orig_sink);
			}
			else
			{
				auto cur_link = orig_link;
				for (unsigned i = 0; i < latency; i++)
				{
					auto rg = new NodeReg();
					rg->set_name(util::str_con_cat("pipe", std::to_string(fstate.dom_id),
						std::to_string(pipe_no), std::to_string(i)));
					sys->add_child(rg);

					auto link_src = (PortRS*)cur_link->get_src();
					auto link_sink = (PortRS*)cur_link->get_sink();

					cur_link = (LinkRSPhys*)sys->splice(cur_link, 
						rg->get_input(), rg->get_output());

					sys->connect(clock_driver, rg->get_clock_port(), NET_CLOCK);
					flow::splice_carrier_protocol(link_src, link_sink, rg);
					splice_backpressure(link_src, rg->get_input(), rg->get_output(), link_sink);
				}
			}

			// Reset link latency to 0 now that it's been realized
			orig_link->set_latency(0);
			pipe_no++;
		}
	}

	void annotate_timing(FlowStateInner& fstate)
	{
		auto sys = fstate.sys;

		for (auto node : sys->get_nodes())
		{
			node->annotate_timing();
		}
	}

	void treeify_merge_nodes(FlowStateInner& fstate)
	{
		// TODO Change this later to tech-dependent
		constexpr unsigned MAX_INPUTS = 4;

		if (genie::impl::get_flow_options().no_merge_tree)
			return;

		auto sys = fstate.sys;
		auto& link_rel = sys->get_link_relations();

		auto all_merges = sys->get_children_by_type<NodeMerge>();

		for (auto orig_mg : all_merges)
		{
			// Gather oroginal inputs
			auto orig_inp_ep = orig_mg->get_endpoint(NET_TOPO, Port::Dir::IN);
			auto orig_inputs = orig_inp_ep->links();
			if (orig_inputs.size() <= MAX_INPUTS)
				continue;

			// Disconnect them
			for (auto input : orig_inputs)
			{
				input->disconnect_sink();
			}

			// Set of inputs to current tree level,
			// initialized to original merge's inputs
			auto cur_inputs = orig_inputs;

			for (unsigned cur_lvl = 0; cur_inputs.size() > MAX_INPUTS ; cur_lvl++)
			{
				// Set of ouputs of current tree level (aka inputs of next)
				decltype(cur_inputs) cur_outputs = {};

				// Number of merge nodes in this tree level
				unsigned n_merges = (cur_inputs.size() + MAX_INPUTS - 1) / MAX_INPUTS;

				for (unsigned new_mg_i = 0; new_mg_i < n_merges; new_mg_i++)
				{
					// This merge node gets a fair share of however many
					// inputs are remaining at this level
					unsigned inputs_this_merge = cur_inputs.size() / (n_merges - new_mg_i);

					// Get our share of inputs, remove them from cur_inputs
					decltype(cur_inputs) this_inputs;
					{
						auto inps_begin = cur_inputs.end() - inputs_this_merge;
						auto inps_end = cur_inputs.end();
						this_inputs.assign(inps_begin, inps_end);
						cur_inputs.erase(inps_begin, inps_end);
					}

					// Create new merge node
					NodeMerge* mg = new NodeMerge();
					mg->set_name(util::str_con_cat(orig_mg->get_name(), "TREE",
						std::to_string(cur_lvl), std::to_string(new_mg_i)));

					sys->add_child(mg);

					// Connect this_inputs to it
					auto mg_inp_ep = mg->get_endpoint(NET_TOPO, Port::Dir::IN);
					for (auto input : this_inputs)
					{
						input->set_sink_ep(mg_inp_ep);
						mg_inp_ep->add_link(input);
					}

					// Create a dangling connection from the output
					auto this_output = sys->connect(mg, nullptr, NET_TOPO);

					// Route logical links from inputs over it
					for (auto input : this_inputs)
					{
						auto logicals = link_rel.get_parents(input->get_id(), NET_RS_LOGICAL);
						for (auto log : logicals)
							link_rel.add(log, this_output->get_id());
					}

					// Add to list of outputs of this level
					cur_outputs.push_back(this_output);
				} // end foreach merge

				// Make this level's outputs next level's inputs
				cur_inputs = std::move(cur_outputs);
			} // end foreach level

			// At this point, cur_inputs has MAX_INPUTS or fewer links.
			// Attach them to the original merge node

			for (auto final_input : cur_inputs)
			{
				final_input->reconnect_sink(orig_inp_ep);
			}
		} // end foreach original merge node
	} // end treeify_merge_nodes()
}

void flow::do_inner(NodeSystem* sys, unsigned dom_id, FlowStateOuter* fs_out)
{
	FlowStateInner fstate;
	fstate.dom_id = dom_id;
	fstate.sys = sys;
	fstate.outer = fs_out;

	treeify_merge_nodes(fstate);

	make_domain_addr_rep(fstate);

	realize_topo_links(fstate);
	insert_addr_converters_user(fstate);
	insert_addr_converters_split(fstate);
	do_protocol_carriage(fstate); // all protocol updating is incremental past this point

	connect_clocks(fstate);
	insert_clockx(fstate);

	do_backpressure(fstate); // all backpressure updating is incremental past this point

	annotate_timing(fstate);
	flow::solve_latency_constraints(sys, dom_id);
	realize_latencies(fstate);

	connect_resets(fstate);
	default_eops(fstate);
	default_xmis_ids(fstate);
}