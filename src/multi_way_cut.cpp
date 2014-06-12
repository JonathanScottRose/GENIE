#include <cassert>
#include <algorithm>
#include <stack>
#include <string>
#include "graph.h"

using namespace ct;
using namespace ct::Graphs;

namespace
{
	VList connected_verts(Graph& G, VertexID t)
	{
		VList result;

		VAttr<bool> visited;
		std::stack<VertexID> to_visit;

		to_visit.push(t);
		while (!to_visit.empty())
		{
			VertexID v = to_visit.top();
			to_visit.pop();

			visited[v] = true;
			result.push_back(v);

			VList neigh = G.neigh(v);
			for (auto n : neigh)
			{
				if (!visited[n])
					to_visit.push(n);
			}
		}

		return result;
	}
}

VAttr<VertexID> Graphs::multi_way_cut(Graph G, EAttr<int>& weights, VList T)
{
	VAttr<VertexID> result;

	assert(T.size() > 0);

	// until nterminals == 1
	while (T.size() > 1)
	{
		typedef std::pair<int, Graph> CutType;
		typedef std::unordered_map<VertexID, CutType>::value_type CutType2;
		std::unordered_map<VertexID, CutType> cuts;

		// for each terminal
		for(auto& t : T)
		{
			auto& entry = cuts[t];
			Graph& H = entry.second; 
			int& cut_weight = entry.first;

			// duplicate graph
			H = G;

			// collect all other terminal vertices
			VList otherT = T;
			otherT.erase(std::find(otherT.begin(), otherT.end(), t));
			
			// choose an s from otherT and merge all the terminals in otherT into it
			VertexID s = otherT[0];
			for (unsigned int i = 1; i < otherT.size(); i++)
			{
				H.mergev(otherT[i], s);
			}

			// Merge multiple edges between vertices v1,v2 into one edge with combined weight
			auto merged_weights = weights;
			for (auto& v : H.verts())
			{
				VList neigh = H.neigh(v);
				for (auto& u : neigh)
				{
					EList edges = H.edges(u, v);
					EdgeID e1 = edges[0];
					for (unsigned int i = 1; i < edges.size(); i++)
					{
						EdgeID e2 = edges[i];
						merged_weights[e1] += merged_weights[e2];
						H.dele(e2);
					}
				}
			}
			
			/*
			H.dump("H" + std::to_string(t), [&](EdgeID e) 
			{ 
				return std::to_string(merged_weights[e]); 
			});
			*/

			// call st mincut with s,t on H and get: total weight, residue grpah
			cut_weight = min_st_cut(H, merged_weights, t, s);

			// results are remembered in cuts map
		}

		// Find minimum s-t mincut
		auto smallest_cut = std::min_element(cuts.begin(), cuts.end(), 
			[](const CutType2& lhs, const CutType2& rhs) { return lhs.second.first < rhs.second.first; }
			);
		
		// The terminal vertex for the minimum mincut, and the residual graph
		VertexID min_terminal = smallest_cut->first;
		Graph& R = smallest_cut->second.second;
		
		// Find all vertices connected to min_terminal in R (including min_terminal itself)
		VList connected = connected_verts(R, min_terminal);

		// For all those connected vertices, paint them as belonging to the terminal vertex min_terminal
		// Also: remove them from G (our copy) permanently
		for (auto v : connected)
		{
			result[v] = min_terminal;
			G.delv(v);
		}

		// Remove terminal min_terminal from the list of terminals
		T.erase(std::find(T.begin(), T.end(), min_terminal));
	}

	// Only one terminal left - assign remaining vertices to this terminal
	for (auto& v : G.verts())
	{
		result[v] = T.front();
	}

	return result;
}