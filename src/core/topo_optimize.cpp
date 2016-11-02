#include <unordered_set>
#include "genie/genie.h"
#include "genie/node_merge.h"
#include "genie/node_split.h"
#include "genie/net_rs.h"

using namespace genie;

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

void measure_latency(std::vector<std::vector<RSLink*>>& lists,
    std::unordered_map<RSLink*, float>& result)
{
    unsigned n_inputs = lists.size();

    // Within each list
    for (unsigned i = 0; i < n_inputs; i++)
    {
        auto& this_list = lists[i];

        // Get each transmission to compete with the biggest transmissions from the other inputs
        for (auto this_xmis : this_list)
        {
            float avg_latency = 0;

            auto excl = this_xmis->asp_get<ARSExclusionGroup>();

            // Go through the other lists
            for (unsigned j = 0; j < n_inputs; j++)
            {
                if (i == j) continue;

                // Add up the total packet sizes of the transmissions that
                // we have a chance of conflicting with
                float pktsize_sum = 0;
                unsigned pktsize_num = 1; // count the 'null' case

                auto& other_list = lists[j];
                for (auto other_xmis : other_list)
                {
                    if (!excl || !excl->has(other_xmis))
                    {
                        pktsize_sum += other_xmis->get_pkt_size();
                        pktsize_num++;
                        break;
                    }
                }

                avg_latency += pktsize_sum / (float)pktsize_num;
            }

            // Add the worst case competition delay
            result[this_xmis] += avg_latency;
        } // end foreach transmission in list
    } // end foreach list
}

void measure_latency_for_merge_node(NodeMerge* mg, 
    std::unordered_map<RSLink*, float>& result)
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
                auto rs_links = util::container_cast<List<RSLink*>>(ac->get_all_parent_links(NET_RS));
                xmis_by_input[i] = rs_links;
            }
            i++;
        }
    }

    measure_latency(xmis_by_input, result);
}

void optimize_domain(System* sys, std::vector<NodeMerge*>& merges)
{
    // Global cumulative latency within this domain. This never gets
    // updated.
    std::unordered_map<RSLink*, float> baseline_latency;

    // Incremental latency per merge node. This is initialized based
    // on the pre-optimization topology, and is updated during optimization.
    std::unordered_map<NodeMerge*, 
        std::unordered_map<RSLink*, float>> incremental_latency;

    // Populate these initial things
    for (auto& mg : merges)
    {
        auto& incr = incremental_latency[mg];
        measure_latency_for_merge_node(mg, incr);

        // Add to cumulative
        for (auto& entry : incr)
        {
            baseline_latency[entry.first] += entry.second;
        }
    }

    // This copy gets updated
    auto cur_total_latency = baseline_latency;

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

            // List of: list of transmissions on each input port from both merge nodes.
            // Initialized to the ones from only the first merge node.
            std::vector<std::vector<RSLink*>> combined_rslinks;

            // A set containing the sources of node1_topos.
            // We will check node2's sources against this.
            std::unordered_set<Port*> node1_srces;
            for (auto topo : node1_topos)
            {
                node1_srces.insert(topo->get_src());
                auto rs_links = topo->asp_get<ALinkContainment>()->get_all_parent_links(NET_RS);
                combined_rslinks.push_back(util::container_cast<List<RSLink*>>(rs_links));
            }

            for (auto it2 = it1 + 1; it2 != merges.end(); ++it2)
            {
                NodeMerge* node2 = *it2;

                if (!node2->asp_has<AAutoGen>())
                    continue;

                // Incoming topo links of second merge node
                auto& node2_topos = node2->get_topo_input()->get_endpoint_sysface(NET_TOPO)->links();

                // Reset combined_rslinks to only contain the things from node1, by just resizing the vector
                combined_rslinks.resize(node1->get_n_inputs());

                // Add the things from node2
                for (auto topo : node2_topos)
                {
                    // Avoid duplicate sources
                    auto src = topo->get_src();
                    if (node1_srces.count(src))
                        continue;

                    auto rs_links = topo->asp_get<ALinkContainment>()->get_all_parent_links(NET_RS);
                    combined_rslinks.push_back(util::container_cast<List<RSLink*>>(rs_links));
                }

                // Measure the incremental contention latencies in the hypothetical combined merge node
                std::unordered_map<RSLink*, float> combined_inc_latency;
                measure_latency(combined_rslinks, combined_inc_latency);
         
                // Using the measured incremental latency on the combined node,
                // the original incremental latency pre-combining, and the original end-to-end latency,
                // calculate the new end-to-end hypthetical latency and see by how much it violates
                // importance constraints.
                
                bool violation = false;
                float this_cost = 0.0f;

                for (auto& new_comb_it : combined_inc_latency)
                {
                    auto rslink = new_comb_it.first;
                    float new_incr = new_comb_it.second;
                
                    // Original end-to-end average latency
                    float old_baseline = baseline_latency[rslink];
                    float old_total = cur_total_latency[rslink];
                        
                    // Find original incremental contribution to that value, pre-combine.
                    // This transmission belongs to either node1 or node2, try both
                    auto& incrs1 = incremental_latency[node1];

                    auto old_incr_loc = incrs1.find(rslink);
                    if (old_incr_loc == incrs1.end())
                    {
                        auto& incrs2 = incremental_latency[node2];
                        old_incr_loc = incrs2.find(rslink);
                    }

                    float old_incr = old_incr_loc->second;

                    // Calculate the new hypothetical total latency
                    float new_total = old_total - old_incr + new_incr;

                    // See if it violates importance constraints
                    float link_imp = rslink->get_importance();
                    unsigned link_size = rslink->get_pkt_size();

                    // Check if any link constraint is violated
                    float ratio = (link_size + old_baseline) / (link_size + new_total);
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
                    if (this_cost > best_cost)
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

        // Delete the second merge node.
        sys->delete_object(best_node2->get_name());

        // Recalculate the incremental and global delays
        std::unordered_map<RSLink*, float> new_incr1_delays;
        measure_latency_for_merge_node(best_node1, new_incr1_delays);

        for (auto& entry : new_incr1_delays)
        {
            auto rslink = entry.first;
            auto new_incr = entry.second;

            float old_incr = incremental_latency[best_node1][rslink];

            cur_total_latency[rslink] += new_incr - old_incr;
        }

        incremental_latency[best_node1] = new_incr1_delays;

    } // end while there are still merge nodes to merge
}

}




namespace genie
{
namespace flow
{

void topo_optimize(System* sys)
{
    // Measure the baseline level of contention-induced latency
    //std::unordered_map<RSLink*, float> cur_delays;
    //measure_latency(sys, cur_delays);

    // Sort merge nodes into domains
    auto all_merges = sys->get_children_by_type<NodeMerge>();
    std::unordered_map<int, std::vector<NodeMerge*>> merge_by_domain;

    for (auto mg : all_merges)
    {
        auto rs_links = RSLink::get_from_topo_port(mg->get_topo_output());
        auto domain = rs_links.front()->get_domain_id();

        merge_by_domain[domain].push_back(mg);
    }

    for (auto& entry : merge_by_domain)
    {
        optimize_domain(sys, entry.second);
    }
}

void topo_optimize_measure_final(System* sys, std::unordered_map<RSLink*, float>& result)
{
    // Measure the latency due to contention of every logical link

    // Initialize
    for (auto& link : sys->get_links(NET_RS))
    {
        auto rs_link = (RSLink*)link;
        result[rs_link] = 0;
    }

    // Get all merge nodes, where contention hapens
    auto mg_nodes = sys->get_children_by_type<NodeMerge>();

    // For each merge node
    for (auto mg_node : mg_nodes)
    {
        measure_latency_for_merge_node(mg_node, result);
    }       
}

}
}