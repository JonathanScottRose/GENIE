#include "pch.h"
#include "graph.h"

using namespace genie::impl;

// Given an undirected graph G with edge weights stored in 'cap', this finds the minimal-weight
// graph cut between vertices s and t.
//
// Graph G is modified to remove the edges of the cut, and the total weight of the cut (sum of the
// weights of the removed edges) is returned.

int graph::min_st_cut(Graph& G, E2Attr<int> cap, VertexID s, VertexID t)
{
	// R is G but with each undirected edge {u,v} converted into two directed edges (u,v) and (v,u).
	// It is the 'residual' graph that gets continually updated by the algorithm as edges get removed.
	Graph R = G;
	
	// turn undirected graph g into a directed graph by treating each existing edge as directed
	// and creating a duplicate edge in the opposite direction - with the same capacity/weight
	for (EdgeID e : G.edges())
	{
		// grab the pair of vertices of each edge and swap them to create the reverse edge
		auto vs = G.verts(e);
		EdgeID newe = R.newe(vs.second, vs.first);

		// update our copy of the capacities map to include a capacity for this new edge
		cap[newe] = cap[e];
	}

	// while a path exists from s to t with >0 residual capacity, use up capacity on that path.
	V2Attr<bool> visited;
	while(true)
	{
		// This makes sure we only visit each vertex once during depth-first traversal.
		// The map is declared outside the loop for performance
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

			// We've reached the the end of the path, t
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
			// Traverse the path to find minimum residual capacity
			int mincap = std::numeric_limits<int>::max();
			VertexID v1 = path[0];
			for (unsigned int i = 1; i < path.size(); i++)
			{
				VertexID v2 = path[i];
				EdgeID e = R.dir_edge(v1, v2);
				mincap = std::min(mincap, cap[e]);
				v1 = v2;
			}

			// Adjust residual capacities along path. The edges that had mincap as their cap
			// will now have 0.
			v1 = path[0];
			for (unsigned int i = 1; i < path.size(); i++)
			{
				VertexID v2 = path[i];
				EdgeID e1 = R.dir_edge(v1, v2);
				EdgeID e2 = R.dir_edge(v2, v1);

				// Forward edge gets reduced capacity, backwards edge gets increased capacity.
				// This is the trick that allows us to use this algorithm with undirected graphs.
				cap[e1] -= mincap;
				cap[e2] += mincap;
				v1 = v2;
			}
		}
		else
		{
			// no path found? done
			break;
		}
	}

	// Remove all zero-weight edges (and their corresponding back-edges) from R.
	// Iterate over edges in G so that 'e' is always an existing forward edge (existing in both
	// G and R) and e2 is a backward edge (existing and only introduced in R).
	//
	// Also, update the capacity of each forward edge 'e' to be the original cost
	for (EdgeID e : G.iter_edges)
	{
		EdgeID e2 = R.redge(e);
		int cap_e = cap[e];
		int cap_e2 = cap[e2];

		if (cap_e == 0 || cap_e2 == 0)
		{
			R.dele(e);
			R.dele(e2);

			// This works because either cap_e or cap_e2 is 0, and the other is equal to
			// the original weight * 2.
			cap[e] = (cap_e + cap_e2) / 2;
		}
	}

	// Find all vertices reachable from s in R
	auto reachable = R.connected_verts(s);
	std::unordered_set<VertexID> reachable_set(reachable.begin(), reachable.end());

	// Find the edges in G that comprise the min-cut.
	// Return the total sum of their weights.
	// Remove those edges from G.
	//
	// A cut edge is an edge e=(u,v) in G such that satisfies both:
	//	 - either v or u is reachable from terminal vertex s
	//   - either e itself or e2(v,u) does not exist in R
	//
	// This represents edges that have zero residual capacity (in either the forward or backward
	// direction) and that lie on the boundary of the cut (one vertex still reachable from s)

	int result = 0;
	auto g_edges = G.edges();
	for (EdgeID e_g : g_edges)
	{
		VPair verts = G.verts(e_g);

		// Only consider edges in G that have at least one vertex in the reachable set
		if (!reachable_set.count(verts.first) && !reachable_set.count(verts.second))
			continue;

		// Try to get edges (u,v) and (v,u) in R
		EdgeID e_r1 = R.dir_edge(verts.first, verts.second);
		EdgeID e_r2 = R.dir_edge(verts.second, verts.first);

		// If either one is missing in R, then this is a cut edge
		if (e_r1 == INVALID_E || e_r2 == INVALID_E)
		{
			// Add the original capacity of the edge in G to the total cut weight.
			result += cap[e_g];

			// Remove the edge from the original input graph. This is the only place where
			// G is modified. 
			G.dele(e_g);
		}
	}	

	return result;
}
