#include <cassert>
#include <algorithm>
#include <stack>
#include "graph.h"

using namespace ct;
using namespace ct::Graphs;

int Graphs::min_st_cut(Graph& G, EAttr<int> cap, VertexID s, VertexID t)
{
	Graph R = G;
	
	// turn undirected graph g into a directed graph by treating each existing edge as directed
	// and creating a duplicate edge in the opposite direction - with the same capacity
	
	for (EdgeID e : G.edges())
	{
		auto vs = G.verts(e);
		EdgeID newe = R.newe(vs.second, vs.first);
		cap[newe] = cap[e];
	}

	// while a path exists from s to t with >0 residual capacity, use up capacity on that path
	VAttr<bool> visited;
	while(true)
	{
		for (VertexID v : R.verts())
		{
			visited[v] = false;
		}

		// Do a depth-first search from s to t to find an augmenting path
		std::vector<VertexID> path;
		path.push_back(s);

		while(!path.empty())
		{
			VertexID cur_v = path.back();
			visited[cur_v] = true;

			if (cur_v == t)
				break;

			// Follow edges that have nonzero remaining capacity
			bool found = false;
			for (EdgeID e : R.edges(cur_v))
			{
				VPair ev = R.verts(e);

				// To do this, each edge of cur_v must:
				// - be directed away from cur_v to the other vertex
				// - have nonzero remaining capacity
				// - point to a vertex that's unvisited
				if (cap[e] > 0 && cur_v == ev.first && !visited[ev.second])
				{
					found = true;
					path.push_back(ev.second);
					break;
				}
			}

			if (!found)
			{
				// Backtrack
				path.pop_back();
			}
		}

		// Found a path?
		if (!path.empty())
		{
			// Find minimum remaining capacity
			int mincap = std::numeric_limits<int>::max();
			VertexID v1 = path[0];
			for (unsigned int i = 1; i < path.size(); i++)
			{
				VertexID v2 = path[i];
				EdgeID e = R.edge(v1, v2);
				mincap = std::min(mincap, cap[e]);
				v1 = v2;
			}

			// Adjust residual capacities along path
			v1 = path[0];
			for (unsigned int i = 1; i < path.size(); i++)
			{
				VertexID v2 = path[i];
				EdgeID e1 = R.edge(v1, v2);
				EdgeID e2 = R.edge(v2, v1);

				cap[e1] -= mincap;
				cap[e2] += mincap;
			}
		}
		else
		{
			// no path found? done
			break;
		}
	}

	// Find total min cut weight and remove cut edges from input graph G
	int result = 0;
	for (EdgeID e : R.edges())
	{
		if (cap[e] == 0)
		{
			VPair verts = R.verts(e);
			EdgeID e2 = R.dir_edge(verts.second, verts.first); // get reverse edge
			result += cap[e2] / 2; // capacity in reverse direction must equal initial weight * 2
			
			EdgeID e_orig = G.edge(verts.first, verts.second); // get undirected edge in original graph
			G.dele(e_orig);
		}
	}

	return result;
}
