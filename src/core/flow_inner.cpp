#include "pch.h"
#include "node_system.h"
#include "node_split.h"
#include "node_merge.h"
#include "net_topo.h"
#include "net_rs.h"
#include "port_rs.h"
#include "flow.h"
#include "genie/port.h"

using namespace genie::impl;
using genie::Exception;
using Dir = genie::Port::Dir;

namespace
{
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

		// Go over all topological links and realize into physical RS links
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
			else if (auto rs = dynamic_cast<PortRS*>(topo_src))
			{
				rs_sink = rs;
			}
			else
			{
				assert(false);
			}

			// Connect
			auto rs_link = dynamic_cast<LinkRS*>(sys->connect(rs_src, rs_sink, NET_RS));

			// Do stuff with link... associate... i dunno
		}
	}
}

void flow::do_inner(NodeSystem* sys)
{
	realize_topo_links(sys);

}