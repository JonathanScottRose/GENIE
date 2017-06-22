#include "pch.h"
#include "topo_optimize.h"
#include "node_system.h"
#include "node_merge.h"
#include "net_rs.h"

using namespace genie;
using namespace impl;
using namespace topo_opt;
/*
namespace std
{
    template<> class hash<pair<int,int>>
    {
    public:
        size_t operator() (const pair<int,int>& p) const
        {
            hash<int> h;
            return h(p.first) ^ h(p.second);
        }
    };
}

namespace
{
	void process_contention_between_links(const std::vector<LinkID>& links,
		ContentionMap& result, NodeSystem* sys, flow::FlowStateOuter& fstate)
	{
		auto& excl = sys->get_link_exclusivity();

		// Pairwise comparison between all links
		for (auto it1 = links.begin(), it_end = links.end(); it1 != it_end; ++it1)
		{
			auto link1 = *it1;
			auto xmis1 = fstate.get_transmission_for_link(link1);
			auto linkp1 = static_cast<LinkRSLogical*>(sys->get_link(link1));
			unsigned pkt1 = linkp1->get_packet_size();
			unsigned contention1 = 0;

			for (auto it2 = it1 + 1; it2 != it_end; ++it2)
			{
				auto link2 = *it2;
				auto xmis2 = fstate.get_transmission_for_link(link2);

				bool same_transmission = xmis1 == xmis2;
				bool are_excl = excl.are_exclusive(link1, link2);

				if (!same_transmission && !are_excl)
				{
					// They contend. Update contention map.
					// Contention = other guy's packet size
					auto linkp2 = static_cast<LinkRSLogical*>(sys->get_link(link2));
					unsigned pkt2 = linkp2->get_packet_size();
					
					// Update link2's contention
					result[link2] += pkt1;

					// Defer updating link1's contention
					contention1 += pkt2;
				}
			}

			result[link1] += contention1;
		}
	}



	void process_contention_for_merge_node(NodeMerge* mg,
		ContentionMap& result)
	{
		// Keep a list of transmissions by input
		std::vector<std::vector<RSLink*>> xmis_by_input;

		auto mg_in = mg->get_topo_input();
		const auto& topo_links = mg_in->get_endpoint_sysface(NET_TOPO)->links();

		unsigned n_inputs = topo_links.size();
		xmis_by_input.resize(n_inputs);

		// Populate and sort lists
		{
			unsigned i = 0;

			for (auto topo_link : topo_links)
			{
				auto ac = topo_link->asp_get<ALinkContainment>();
				if (ac)
				{
					auto rs_links = ac->get_all_parent_links(NET_RS);
					gather_unique_xmis(rs_links, xmis_by_input[i]);
				}
				i++;
			}
		}

		measure_contention(xmis_by_input, result);
	}
}

ContentionMap topo_opt::measure_initial_contention(NodeSystem* sys, flow::FlowStateOuter& fstate)
{
	ContentionMap result;

	// Measures the intial contention between the system's logical links when there's a crossbar topology.
	// Every logical sink is fed by a merge node, so all transmissions arriving at a sink must compete with
	// each other (unless exclusive). We only need to look at all the transmissions arriving at each sink
	// to determine contention.

	// Sorts logical RS links by destination
	std::unordered_map<HierObject*, std::vector<LinkID>> dest_to_logicals;

	for (auto link : sys->get_links(NET_RS_LOGICAL))
	{
		dest_to_logicals[link->get_src()].push_back(link->get_id());
	}

	// Look within each sink for a list of rslogicals.
	// Find contention within each list.
	for (auto dest : dest_to_logicals)
	{
		process_contention_between_links(dest.second, result,
			sys, fstate);
	}
	
	return result;
}


void fix_split(System* sys, NodeSplit* sp)
{
    // 'sp' should have two downstream nodes. Get ports
    auto sp_topo_out = sp->get_topo_output();
    auto sp_topo_out_ep = sp_topo_out->get_endpoint_sysface(NET_TOPO);
    auto sp_downstream_ports = sp_topo_out_ep->get_remote_objs();

    assert(sp_downstream_ports.size() == 2);

    // For each of the two downstream nodes
    for (auto sp_ds_port : sp_downstream_ports)
    {
        auto sp_ds_node = as_a<NodeSplit*>(sp_ds_port->get_node());

        // Check if it's a split node that we're allowed to touch
        if (!sp_ds_node)
            continue;

        // Disconnect 'sp' from this split node
        sys->disconnect(sp_topo_out, sp_ds_port);

        // Get downstream links of this split node
        auto sp_ds_node_out = sp_ds_node->get_topo_output();
        auto reroute_links = sp_ds_node_out->get_endpoint_sysface(NET_TOPO)->links();

        // Reconnect each link's source to be 'sp'
        for (auto reroute_link : reroute_links)
        {
            reroute_link->set_src(sp_topo_out_ep);
            sp_topo_out_ep->add_link(reroute_link);
        }

        // Delete downstream split node
        sys->delete_object(sp_ds_node->get_name());
    }
}

void measure_contention(std::vector<std::vector<RSLink*>>& lists,
    std::unordered_map<int, unsigned>& result)
{
    unsigned n_inputs = lists.size();

    // Within each list
    for (unsigned i = 0; i < n_inputs; i++)
    {
        auto& this_list = lists[i];

        for (auto this_xmis : this_list)
        {
            unsigned total_contention = 0;

            auto excl = this_xmis->asp_get<ARSExclusionGroup>();

            // Go through the other lists
            for (unsigned j = 0; j < n_inputs; j++)
            {
                if (i == j) continue;

                // Add packet sizes of contending transmissions to total contention
                auto& other_list = lists[j];
                for (auto other_xmis : other_list)
                {
                    bool conflict = !excl || !excl->has(other_xmis);
                    bool same_xmis = this_xmis->get_flow_id() == other_xmis->get_flow_id();
                    if (conflict && !same_xmis)
                    {
                        total_contention += other_xmis->get_pkt_size();
                    }
                }

            }

            // Add the worst case competition delay
            int this_fid = this_xmis->get_flow_id();
            result[this_fid] += total_contention;
        } // end foreach transmission in list
    } // end foreach list
}

void gather_unique_xmis(std::vector<Link*>& in, std::vector<RSLink*>& out)
{
    unsigned n = in.size();

    for (unsigned i = 0; i < n; i++)
    {
        auto link_rs = (RSLink*)in[i];

        int i_fid = link_rs->get_flow_id();
        bool found = false;

        for (auto link_existing : out)
        {
            int j_fid = link_existing->get_flow_id();
            if (i_fid == j_fid)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            out.push_back(link_rs);
        }
    }
}



void optimize_domain(System* sys, std::vector<NodeMerge*>& merges,
    std::unordered_map<int, std::vector<RSLink*>>& fid_to_links)
{
    // Global cumulative latency within this domain. This never gets
    // updated.
    std::unordered_map<int, unsigned> baseline_contention;

    // Incremental latency per merge node. This is initialized based
    // on the pre-optimization topology, and is updated during optimization.
    std::unordered_map<NodeMerge*, 
        std::unordered_map<int, unsigned>> incremental_contention;

    // Populate these initial things
    for (auto& mg : merges)
    {
        auto& incr = incremental_contention[mg];
        measure_contention_for_merge_node(mg, incr);

        // Add to cumulative
        for (auto& entry : incr)
        {
            baseline_contention[entry.first] += entry.second;
        }
    }

    // This copy gets updated
    auto cur_total_contention = baseline_contention;

    // While can still combine more merge nodes...
    while(true)
    {
        // Keep track of whether we found a legal mergemerge candidate pair,
        // the members of the best pair, and the best pair's cost.
        bool best_found = false;
        float best_cost = 0;
        NodeMerge* best_node1 = nullptr;
        NodeMerge* best_node2 = nullptr;

        // Loop through all pairs and find the best pair to merge
        for (auto it1 = merges.begin(); it1 != merges.end(); ++it1)
        {
            NodeMerge* node1 = *it1;

            if (!node1->asp_has<AAutoGen>())
                continue;

            // Incoming topo links of first merge node
            auto& node1_topos = node1->get_topo_input()->get_endpoint_sysface(NET_TOPO)->links();

            // Get lists of RS links, sorted by their physical source
            std::unordered_map<Port*, std::vector<RSLink*>> node1_rslinks;
            for (auto topo_link : node1_topos)
            {
                auto rs_links = topo_link->asp_get<ALinkContainment>()->get_all_parent_links(NET_RS);
                auto topo_src = topo_link->get_src();
                gather_unique_xmis(rs_links, node1_rslinks[topo_src]);
            }

            for (auto it2 = it1 + 1; it2 != merges.end(); ++it2)
            {
                NodeMerge* node2 = *it2;

                if (!node2->asp_has<AAutoGen>())
                    continue;

                // Incoming topo links of second merge node
                auto& node2_topos = node2->get_topo_input()->get_endpoint_sysface(NET_TOPO)->links();

                // Initialize to copy of node1's. Add node2's stuff.
                auto combined_rslinks = node1_rslinks;

                // Add the things from node2
                for (auto topo_link : node2_topos)
                {
                    auto rs_links = topo_link->asp_get<ALinkContainment>()->get_all_parent_links(NET_RS);
                    auto topo_src = topo_link->get_src();
                    gather_unique_xmis(rs_links, combined_rslinks[topo_src]);
                }

                // Ugly: convert combined_rslinks into a vector or vectors
                std::vector<std::vector<RSLink*>> cmb_rslinks_vec;
                for (auto& bin : combined_rslinks)
                {
                    cmb_rslinks_vec.push_back(bin.second);
                }

                // Measure the incremental contention latencies in the hypothetical combined merge node
                std::unordered_map<int, unsigned> combined_inc_contention;
                measure_contention(cmb_rslinks_vec, combined_inc_contention);
         
                // Using the measured incremental latency on the combined node,
                // the original incremental latency pre-combining, and the original end-to-end latency,
                // calculate the new end-to-end hypthetical latency and see by how much it violates
                // importance constraints.
                
                bool violation = false;
                float this_cost = 0;

                for (auto& new_comb_it : combined_inc_contention)
                {
                    auto fid = new_comb_it.first;
                    unsigned new_incr = new_comb_it.second;
                
                    // Original total contention
                    unsigned old_baseline = baseline_contention[fid];
                    unsigned old_total = cur_total_contention[fid];
                        
                    // Find original incremental contribution to that value, pre-combine.
                    // This transmission can appear in node1 AND/OR node2, so sum up both

                    unsigned old_incr = 0;

                    for (auto node : {node1, node2})
                    {
                        auto& incrs = incremental_contention[node];
                        auto old_incr_loc = incrs.find(fid);

                        if (old_incr_loc != incrs.end())
                        {
                            old_incr += old_incr_loc->second;
                        }
                    }

                    // Calculate the new hypothetical total contention
                    unsigned new_total = old_total - old_incr + new_incr;

                    // See if it violates importance constraints
                    auto rslink = fid_to_links[fid].front();
                    float link_imp = rslink->get_importance();
                    unsigned link_size = rslink->get_pkt_size();

                    // Check if any link constraint is violated
                    float ratio = (link_size + old_baseline) / (float)(link_size + new_total);
                    if (ratio < link_imp)
                    {
                        violation = true;
                        break;
                    }
                    else
                    {
                        // Add up the margins
                        this_cost += ratio - link_imp;
                    }
                }

                // If any of the links' costs were violated, this isn't a valid combine candidate pair
                if (violation)
                {
                    continue;
                }
                else
                {
                    if (!best_found || this_cost > best_cost)
                    {
                        best_found = true;
                        best_cost = this_cost;
                        best_node1 = node1;
                        best_node2 = node2;
                    }
                }
            }// end loop through 'other' merge nodes
        } // end looping through all pairs of merge nodes

        // If a legal mergemerge candidate was found, focus on the two 'best' merge nodes
        // and perform the actual mergemerge operation.
        //
        // If not? Then escape from outermost while loop and return from this function
        if (!best_found)
        {
            break;
        }

        // Arbitrarily choose which of the two merge nodes will remain, as the other one will
        // be destroyed. Let's make node1 the survivor

        // Grab all the input links of the two merge nodes
        auto node1_inputs = best_node1->get_topo_input()->get_endpoint_sysface(NET_TOPO)->links();
        auto node2_inputs = best_node2->get_topo_input()->get_endpoint_sysface(NET_TOPO)->links();

        // Get the transmissions (RS Links) on each topo link (used later)
        std::vector<std::vector<Link*>> node1_rslinks, node2_rslinks;

        for (auto tlink : node1_inputs)
        {
            auto links = tlink->asp_get<ALinkContainment>()->get_all_parent_links(NET_RS);
            node1_rslinks.push_back(links);
        }

        for (auto tlink : node2_inputs)
        {
            auto links = tlink->asp_get<ALinkContainment>()->get_all_parent_links(NET_RS);
            node2_rslinks.push_back(links);
        }

        // Disconnect node2's input links and reconnect their sources to node1's input
        for (unsigned i = 0; i < node2_inputs.size(); i++)
        {
            auto input = node2_inputs[i];
            auto input_src = input->get_src();

            sys->disconnect(input);
            auto link = sys->connect(input_src, best_node1->get_topo_input());

            // Move transmissions
            link->asp_get<ALinkContainment>()->add_parent_links(node2_rslinks[i]);
        }

        // Combine outputs: create a split node. its input: output of remaining merge node
        // its outputs: original outputs of two merge nodes
        {

            NodeSplit* sp = new NodeSplit();
            sp->set_name(sys->make_unique_child_name("split_auto"));
            sp->asp_add(new AAutoGen);
            sys->add_child(sp);

            // Get merge node destination links
            auto node1_out = best_node1->get_topo_output()->get_endpoint_sysface(NET_TOPO)->get_link0();
            auto node2_out = best_node2->get_topo_output()->get_endpoint_sysface(NET_TOPO)->get_link0();

            auto node1_sink = node1_out->get_sink();
            auto node2_sink = node2_out->get_sink();

            // Disconnect the outputs of the merge nodes
            sys->disconnect(node1_out);
            sys->disconnect(node2_out);

            // Split node's input is merge node1's output
            auto mg_to_sp = sys->connect(best_node1->get_topo_output(), sp->get_topo_input());

            // Split node's outputs go to original merge node outputs
            auto sp_to_sink1 = sys->connect(sp->get_topo_output(), node1_sink);
            auto sp_to_sink2 = sys->connect(sp->get_topo_output(), node2_sink);

            // Re-route carried RS links
            for (auto rslinks : node1_rslinks)
            {
                mg_to_sp->asp_get<ALinkContainment>()->add_parent_links(rslinks);
                sp_to_sink1->asp_get<ALinkContainment>()->add_parent_links(rslinks);
            }

            for (auto rslinks : node2_rslinks)
            {
                mg_to_sp->asp_get<ALinkContainment>()->add_parent_links(rslinks);
                sp_to_sink2->asp_get<ALinkContainment>()->add_parent_links(rslinks);
            }

            // Combine downstream split nodes with the new split node, if possible
            fix_split(sys, sp);
        }

        // Recalculate the incremental and global delays
        std::unordered_map<int, unsigned> new_incr1_contention;
        measure_contention_for_merge_node(best_node1, new_incr1_contention);

        for (auto& entry : new_incr1_contention)
        {
            int fid = entry.first;
            unsigned new_incr = entry.second;
            unsigned old_incr = 0;

            // node2 is going away, and node1 is now the combination of the old node1 and node2.
            // Two operations:
            // 1) Remove the transmission's former contributions (which came from old node1 or old node2 OR BOTH)
            // 2) Add the new contribution based on the new combined node1

            // 1) sum up old node1 and/or old node2's contributions for this flow
            for (auto node : {best_node1, best_node2})
            {
                auto& node_bins = incremental_contention[node];
                auto incr_loc = node_bins.find(fid);
                if (incr_loc != node_bins.end())
                {
                    old_incr += incr_loc->second;
                }
            }

            // Remove 1) and add 2)
            cur_total_contention[fid] += new_incr - old_incr;
        }

        // Delete the second merge node.
        sys->delete_object(best_node2->get_name());
        merges.erase(std::find(merges.begin(), merges.end(), best_node2));
        incremental_contention[best_node1] = new_incr1_contention;
        incremental_contention.erase(best_node2);

    } // end while there are still merge nodes to merge
}

}


*/