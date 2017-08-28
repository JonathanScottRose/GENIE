#include "pch.h"
#include "topo_optimize.h"
#include "node_system.h"
#include "node_merge.h"
#include "node_split.h"
#include "port.h"
#include "net_rs.h"
#include "net_topo.h"

using namespace genie;
using namespace impl;
using namespace flow;
using namespace topo_opt;

using flow::TransmissionID;
using flow::FlowStateOuter;

namespace
{
	// Holds the contention between all pairs of transmissions:
	// - do they contend?
	// - for each transmission, the total contention
	class ContentionMap
	{
	public:
		void init(unsigned count)
		{
			m_count = count;
			m_contend.resize(count*count);
			m_totals.resize(count);

			for (auto& i : m_contend) i = false;
			for (auto& i : m_totals) i = 0;
		}

		void add(TransmissionID t1, TransmissionID t2, float val1, float val2)
		{
			m_contend[idx(t1, t2)] = true;
			m_totals[t1] += val1;
			m_totals[t2] += val2;
		}

		bool do_contend(TransmissionID t1, TransmissionID t2)
		{
			return m_contend[idx(t1, t2)];
		}

		float get_total(TransmissionID t)
		{
			return m_totals[t];
		}

	protected:
		static unsigned idx(TransmissionID t1, TransmissionID t2)
		{
			if (t1 > t2) std::swap(t1, t2);
			return t2*(t2 + 1) / 2 + t1;
		}


		unsigned m_count;
		std::vector<bool> m_contend;
		std::vector<float> m_totals;
	};
}

// Holds context during iteration
struct topo_opt::TopoOptState
{
	FlowStateOuter* fstate;

	ContentionMap xbar_contention;

	NodeSystem* iter_base_sys;
	ContentionMap iter_base_contention;

	std::vector<NodeMerge*> merge_nodes;
	unsigned cur_merge1;
	unsigned cur_merge2;
};

namespace
{
	struct TopoWithLogicalLinks
	{
		LinkID topo;
		std::vector<LinkID> logicals;
	};

	using MergeInputsBySrc = std::map<std::string, TopoWithLogicalLinks>;

	void process_contention_between_links(const std::vector<LinkID>& links1,
		const std::vector<LinkID>& links2,
		NodeSystem* sys, FlowStateOuter* fstate, ContentionMap& result)
	{
		auto& excl = sys->get_spec().excl_info;

		for (auto link1 : links1)
		{
			auto xmis1 = fstate->get_transmission_for_link(link1);
			auto linkp1 = static_cast<LinkRSLogical*>(sys->get_link(link1));
			auto src1 = linkp1->get_src();
			unsigned pkt1 = linkp1->get_packet_size();

			for (auto link2 : links2)
			{
				auto xmis2 = fstate->get_transmission_for_link(link2);

				bool same_transmission = xmis1 == xmis2;
				if (same_transmission)
					continue;

				bool are_excl = excl.are_exclusive(link1, link2);
				if (are_excl)
					continue;

				bool already_contend = result.do_contend(xmis1, xmis2);
				if (already_contend)
					continue;

				auto linkp2 = static_cast<LinkRSLogical*>(sys->get_link(link2));
				auto src2 = linkp2->get_src();

				if (src1 == src2)
					continue;

				// They contend. Update contention map.
				// Contention = other guy's packet size
				unsigned pkt2 = linkp2->get_packet_size();

				// Update contention
				result.add(xmis1, xmis2, (float)pkt1, (float)pkt2);
			}
		}
	} // end function

	void process_contention_between_link_groups(
		const std::vector<std::vector<std::vector<LinkID>>>& groups,
		NodeSystem* sys, FlowStateOuter* fstate, ContentionMap& result)
	{
		// Groups contains:
		//
		// group1:
		// list of links
		// list of links
		//
		// group2:
		// list of links
		// list of links
		// ...
		//
		// group3: ...

		// Pairwise compare between all <list of links> belonging to different groups

		for (auto it_g1 = groups.begin(), it_g_end = groups.end(); it_g1 != it_g_end; ++it_g1)
		{
			auto& group1 = *it_g1;

			for (auto it_g2 = it_g1 + 1; it_g2 != it_g_end; ++it_g2)
			{
				auto& group2 = *it_g2;

				// Iterate through lists of links in group1
				for (auto it_l1 = group1.begin(), it_l1_end = group1.end(); it_l1 != it_l1_end; ++it_l1)
				{
					auto& list1 = *it_l1;

					for (auto it_l2 = group2.begin(), it_l2_end = group2.end(); it_l2 != it_l2_end; ++it_l2)
					{
						auto& list2 = *it_l2;

						process_contention_between_links(list1, list2, sys, fstate, result);
					}
				}
			}
		}
	}

	void process_contention_for_merge_node(NodeSystem* sys, FlowStateOuter* fstate, NodeMerge* mg,
		ContentionMap& result)
	{
		using genie::impl::Port;

		auto& link_rel = sys->get_link_relations();

		// Group logical links by merge node input (links present on same
		// physical input can't compete with each other)
		std::vector<std::vector<std::vector<LinkID>>> loglink_by_input;

		auto mg_in = mg->get_endpoint(NET_TOPO, Port::Dir::IN);
		const auto& topo_links = mg_in->links();

		unsigned n_inputs = topo_links.size();
		loglink_by_input.resize(n_inputs);

		// Populate and sort lists
		{
			auto it_vec = loglink_by_input.begin();

			for (auto topo_link : topo_links)
			{
				it_vec->push_back(link_rel.get_parents(topo_link->get_id(), NET_RS_LOGICAL));
				++it_vec;
			}
		}

		// Process
		process_contention_between_link_groups(loglink_by_input, sys, fstate, result);
	}

	void populate_merge_inputs(NodeMerge* mg, MergeInputsBySrc& out, LinkRelations& rel)
	{
		using impl::Port;

		auto ep = mg->get_endpoint(NET_TOPO, Port::Dir::IN);
		auto& topos = ep->links();
		
		for (auto& topo : topos)
		{
			auto& srcname = topo->get_src()->get_name();
			auto& entry = out[srcname];
			entry.topo = topo->get_id();
			entry.logicals = rel.get_parents(entry.topo, NET_RS_LOGICAL);
		}
	}

	bool evaluate_merge_candidate_topoinputs(TopoOptState* tstate,
		std::vector<LinkID>& topo1,
		std::vector<LinkID>& topo2,
		ContentionMap& cand_contention)
	{
		// Compare logical links from topo1 with those from topo2
		// and calculate contention. This is incremental contention vs.
		// the baseline state when the two merge nodes are not combined.
		//
		// Use the incremental contention, plus base contention, and compare that
		// vs xbar contention and see if this meets importance requirements

		auto fstate = tstate->fstate;
		auto sys = tstate->iter_base_sys;
		auto& spec = sys->get_spec();
		auto& excl_info = spec.excl_info;
		auto& xbar_contention = tstate->xbar_contention;

		for (auto log_id1 : topo1)
		{
			auto xmis1 = fstate->get_transmission_for_link(log_id1);
			auto linkp1 = static_cast<LinkRSLogical*>(sys->get_link(log_id1));
			auto src1 = linkp1->get_src();
			float imp1 = linkp1->get_importance();
			unsigned pkt1 = linkp1->get_packet_size();

			float xbar_cont1 = xbar_contention.get_total(xmis1);

			for (auto log_id2 : topo2)
			{
				auto xmis2 = fstate->get_transmission_for_link(log_id2);
				
				auto same_xmis = xmis1 == xmis2;
				if (same_xmis)
					continue;

				auto are_excl = excl_info.are_exclusive(log_id1, log_id2);
				if (are_excl)
					continue;

				bool already_contend = cand_contention.do_contend(xmis1, xmis2);

				auto linkp2 = static_cast<LinkRSLogical*>(sys->get_link(log_id2));
				auto src2 = linkp2->get_src();

				auto same_src = src1 == src2;
				if (same_src)
					continue;

				float imp2 = linkp2->get_importance();
				unsigned pkt2 = linkp2->get_packet_size();
				float xbar_cont2 = xbar_contention.get_total(xmis2);

				// Calculate an incremental contention between the two transmissions
				float inc_cont1 = (float)pkt2;
				float inc_cont2 = (float)pkt1;

				// Update contention
				cand_contention.add(xmis1, xmis2, inc_cont1, inc_cont2);

				// Get total new contention thus far
				float cand_cont1 = cand_contention.get_total(xmis1);
				float cand_cont2 = cand_contention.get_total(xmis2);
				
				// Do the check
				if (cand_cont1 > 0 && xbar_cont1 / cand_cont1 < imp1)
					return false;

				if (cand_cont2 > 0 && xbar_cont2 / cand_cont2 < imp2)
					return false;
			}
		}

		return true;
	}

	bool evaluate_merge_candidate(TopoOptState* tstate, 
		NodeMerge* mg1,	MergeInputsBySrc& mg1_inputs,
		NodeMerge* mg2,	MergeInputsBySrc& mg2_inputs)
	{
		// Loop through pairwise: inputs from 1, inputs from 2,
		// ignoring pairs of inputs that have the same source

		// Make a copy of the contention map from the iterbase
		ContentionMap cand_contention = tstate->iter_base_contention;

		for (auto& it_mg1 : mg1_inputs)
		{
			auto& mg1_srcname = it_mg1.first;
			auto& mg1_input = it_mg1.second;

			for (auto& it_mg2 : mg2_inputs)
			{
				auto& mg2_srcname = it_mg2.first;
				auto& mg2_input = it_mg2.second;

				// Topo inputs that come from the same src can never conflict
				if (mg1_srcname == mg2_srcname)
					continue;

				if (!evaluate_merge_candidate_topoinputs(tstate, 
					mg1_input.logicals, 
					mg2_input.logicals, 
					cand_contention))
					return false;
			}
		}

		return true;
	}

	void fixup_split_nodes(NodeSystem* sys)
	{
		using impl::Port;
		auto& link_rel = sys->get_link_relations();

		// There's two kinds of things to fix:
		// 1) Split nodes that have only one output need to be removed
		// 2) Split node that feed other split nodes need to be combined

		auto all_sp = sys->get_children_by_type<NodeSplit>();
		std::vector<NodeSplit*> nodes_to_erase;

		// First thing:
		for (auto it = all_sp.begin(); it != all_sp.end(); )
		{
			auto sp = *it;

			if (sp->get_n_outputs() == 1)
			{
				auto ep_in = sp->get_endpoint(NET_TOPO, Port::Dir::IN);
				auto ep_out = sp->get_endpoint(NET_TOPO, Port::Dir::OUT);

				// Remove the split node and outgoing link, replace with the incoming link
				auto link_in = ep_in->get_link0();
				auto link_out = ep_out->get_link0();
				auto ep_downstream = link_out->get_sink_ep();

				link_out->disconnect_sink();
				link_in->disconnect_sink();
				link_in->reconnect_sink(ep_downstream);

				nodes_to_erase.push_back(sp);
			}
			else
			{
				++it;
			}
		}

		for (auto sp : nodes_to_erase)
		{
			delete sys->remove_child(sp);
			all_sp.erase(std::remove(all_sp.begin(), all_sp.end(), sp));
		}

		nodes_to_erase.clear();

		// Second thing:
		for (auto it = all_sp.begin(); it != all_sp.end(); ++it)
		{
			auto upstream_sp = *it;
			std::vector<impl::Link*> links_to_add;
			std::vector<impl::Link*> links_to_remove;

			// Go through all the outputs of the split node, and look for things that
			// are also split nodes.
			auto upstream_sp_out_ep = upstream_sp->get_endpoint(NET_TOPO, Port::Dir::OUT);
			auto& upstream_sp_out_links = upstream_sp_out_ep->links();
			for (auto upstream_sp_out_link : upstream_sp_out_links)
			{
				auto downstream_sp = dynamic_cast<NodeSplit*>(upstream_sp_out_link->get_sink());
				if (!downstream_sp)
					continue;

				// It's a split node. Grab all of its outputs and disconnect from downstream sp
				auto downstream_sp_out_ep = downstream_sp->get_endpoint(NET_TOPO, Port::Dir::OUT);
				auto downstream_sp_out_links = downstream_sp_out_ep->links(); // copy! disconnect_src modifies original
				for (auto downstream_sp_out_link : downstream_sp_out_links)
					downstream_sp_out_link->disconnect_src();

				links_to_add.insert(links_to_add.end(), downstream_sp_out_links.begin(), downstream_sp_out_links.end());

				// Disconnect the link from upstream to downstream sp
				upstream_sp_out_link->disconnect_sink();
				links_to_remove.push_back(upstream_sp_out_link);

				// Kill the downstream split node
				nodes_to_erase.push_back(downstream_sp);
			}

			// Remove upstream_sp's outgoing links to now-dead split nodes
			for (auto link : links_to_remove)
				sys->disconnect(link);

			// Move all outgoing links from destroyed downstream split nodes to upstream_sp
			for (auto link : links_to_add)
				link->reconnect_src(upstream_sp_out_ep);
		}

		for (auto sp : nodes_to_erase)
		{
			delete sys->remove_child(sp);
		}
	}

	NodeSystem* create_combined_sys(TopoOptState* tstate,
		MergeInputsBySrc& mg1_inputs,
		MergeInputsBySrc& mg2_inputs)
	{
		using impl::Port;

		// First, clone the base system
		NodeSystem* sys = (NodeSystem*)tstate->iter_base_sys->clone();
		auto& link_rel = sys->get_link_relations();

		// Get the merge nodes in the new system
		auto mg1 = sys->get_child_as<NodeMerge>(tstate->merge_nodes[tstate->cur_merge1]->get_name());
		auto mg2 = sys->get_child_as<NodeMerge>(tstate->merge_nodes[tstate->cur_merge2]->get_name());

		// We're going to delete the second merge node and reroute its links to the first one.
		
		// Disconnect the second merge node's inputs
		for (auto it : mg2_inputs)
		{
			auto input = sys->get_link(it.second.topo);
			input->disconnect_sink();
		}

		// Get and disconnect the second merge node's output
		auto mg2_output = mg2->get_endpoint(NET_TOPO, Port::Dir::OUT)->get_link0();
		mg2_output->disconnect_src();

		// Get and disconnect the first merge node's output
		auto mg1_output = mg1->get_endpoint(NET_TOPO, Port::Dir::OUT)->get_link0();
		mg1_output->disconnect_src();

		// Remove and destroy the second merge node from the system
		sys->remove_child(mg2->get_name());
		delete mg2;

		// Reconnect mg2 inputs to mg1
		for (auto mg2_input_it : mg2_inputs)
		{
			auto& input_src_name = mg2_input_it.first;
			auto& mg2_input = mg2_input_it.second;

			// Two possibilities:
			// - If mg1 already has an input with the same source, just move over
			// all the contained logical links, and destroy the topo link
			// - If not, then reconnect the topo link to mg1

			auto existing_mg1_it = mg1_inputs.find(input_src_name);
			if (existing_mg1_it != mg1_inputs.end())
			{
				// Get mg1's existing input and reroute the mg2 logical links to it
				auto mg1_topo_id = existing_mg1_it->second.topo;
				for (auto logical : mg2_input.logicals)
				{
					link_rel.add(logical, mg1_topo_id);
				}

				// Destroy the mg2 topo link
				auto mg2_topo_id = mg2_input.topo;
				sys->disconnect(sys->get_link(mg2_topo_id));
			}
			else
			{
				// Get the mg2 topo link
				auto mg2_input_topolink = sys->get_link(mg2_input.topo);

				// Just reconnect it to mg1
				mg2_input_topolink->reconnect_sink(
					mg1->get_endpoint(NET_TOPO, Port::Dir::IN));
			}
		}

		//
		// Output part: Create a split node, connected to the output of mg1.
		// The outputs of the split node will be the original mg1 and mg2 outputs.
		// The link from mg1 to the split node needs all logical links routed over it.
		//

		// Create a split node
		auto sp = new NodeSplit();
		sp->set_name(sys->make_unique_child_name(util::str_con_cat("sp", mg1->get_name())));
		sys->add_child(sp);

		// Connect mg1 and mg2 former outputs, as outputs to sp
		auto sp_out_ep = sp->get_endpoint(NET_TOPO, Port::Dir::OUT);
		mg1_output->reconnect_src(sp_out_ep);
		mg2_output->reconnect_src(sp_out_ep);

		// Create a link from mg1 to sp
		auto mg_sp_link = sys->connect(mg1, sp, NET_TOPO);

		// Route logical links over it
		for (auto topo : { mg1_output, mg2_output })
		{
			auto logicals = link_rel.get_parents(topo->get_id(), NET_RS_LOGICAL);
			for (auto logical : logicals)
				link_rel.add(logical, mg_sp_link->get_id());
		}

		fixup_split_nodes(sys);

		return sys;
	}
}


TopoOptState* topo_opt::init(NodeSystem* xbar_sys,
	flow::FlowStateOuter& fstate)
{
	// Create state
	auto ts = new TopoOptState;

	ts->fstate = &fstate;

	// The first base system is the xbar system
	iter_newbase(ts, xbar_sys);

	// Remember its contention as the xbar contention
	ts->xbar_contention = ts->iter_base_contention;

	return ts;
}

void topo_opt::iter_newbase(TopoOptState* ts, NodeSystem* base)
{
	ts->iter_base_sys = base;
	
	// Get merge nodes
	ts->merge_nodes = base->get_children_by_type<NodeMerge>();

	// Initialize which merge nodes we're going to try combining first.
	// This gets incremented/checked before any combining happens.
	ts->cur_merge1 = 0;
	ts->cur_merge2 = 0;
	
	// Init/reset contention
	ts->iter_base_contention.init(ts->fstate->get_n_transmissions());

	// Measure initial contention
	for (auto mg : ts->merge_nodes)
	{
		process_contention_for_merge_node(base,
			ts->fstate, mg, ts->iter_base_contention);
	}
}

NodeSystem* topo_opt::iter_next(TopoOptState* ts)
{
	NodeSystem* result = nullptr;

	unsigned n_merges = ts->merge_nodes.size();

	for (bool exit_loop = false; !exit_loop; )
	{
		// Handle loop counters
		ts->cur_merge2++;
		if (ts->cur_merge2 >= n_merges)
		{
			// cur_merge2 reached end of its loop, check/advance cur_merge1
			// and reset cur_merge2
			ts->cur_merge1++;
			ts->cur_merge2 = ts->cur_merge1 + 1;
			if (ts->cur_merge1 + 1 >= n_merges) // should be >= n_merges-1 thanks unsigned integers
			{
				// Done iteration : out of merge node pairs
				exit_loop = true;
				continue;
			}
		}

		NodeSystem* sys = ts->iter_base_sys;
		NodeMerge* mg1 = ts->merge_nodes[ts->cur_merge1];
		NodeMerge* mg2 = ts->merge_nodes[ts->cur_merge2];

		// Evaluate the proposed combination of cur_merge1 and cur_merge2
		// Get the topo inputs of both merge nodes, get the logical links going
		// over them, and key the topo inputs by the name of their source
		MergeInputsBySrc mg1_inputs, mg2_inputs;
		populate_merge_inputs(mg1, mg1_inputs, sys->get_link_relations());
		populate_merge_inputs(mg2, mg2_inputs, sys->get_link_relations());

		// Check if combining the two merge nodes will be okay
		if (!evaluate_merge_candidate(ts, mg1, mg1_inputs, mg2, mg2_inputs))
		{
			continue;
		}
		else
		{
			// It is okay! Make the merge-combination happen, and return a new system
			result = create_combined_sys(ts, mg1_inputs, mg2_inputs);
			exit_loop = true;
		}
	}

	return result;
}

void topo_opt::cleanup(TopoOptState* ts)
{
	delete ts;
}

