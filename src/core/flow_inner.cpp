#include "pch.h"
#include "node_system.h"
#include "node_split.h"
#include "node_merge.h"
#include "node_user.h"
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
					if (!output->get_endpoint(NET_RS, Dir::OUT)->is_connected())
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
					if (!input->get_endpoint(NET_RS, Dir::IN)->is_connected())
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
			auto rs_link = static_cast<LinkRS*>(sys->connect(rs_src, rs_sink, NET_RS));

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

		// For every user port (that has a NET_RS link), check for useraddr fields,
		// and insert converters
		for (auto user_port : user_ports)
		{
			// System-facing endpoint
			auto ep = user_port->get_endpoint(NET_RS, user_port->get_effective_dir(sys));
			auto rs_link = static_cast<LinkRS*>(ep->get_link0());
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

				
			}
		}
	}

}

void flow::do_inner(NodeSystem* sys)
{
	sys->set_flow_state_inner(new FlowStateInner);

	create_transmissions(sys);
	flow::make_internal_flow_rep(sys);

	realize_topo_links(sys);
	insert_addr_converters_user(sys);
}