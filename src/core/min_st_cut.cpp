#include <cassert>
#include <algorithm>
#include <stack>
#include "graph.h"

using namespace genie;
using namespace genie::Graphs;

// Given an undirected graph G with edge weights stored in 'cap', this finds the minimal-weight
// graph cut between vertices s and t.
//
// Graph G is modified to remove the edges of the cut, and the total weight of the cut (sum of the
// weights of the removed edges) is returned.

int Graphs::min_st_cut(Graph& G, EAttr<int> cap, VertexID s, VertexID t)
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
	VAttr<bool> visited;
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

	// Find total min cut weight and remove cut edges from input graph G.
	// The edges of the cut are the edges that have zero capacity.
	int result = 0;
	for (EdgeID e : R.edges())
	{
		// Is this edge part of the cut?
		if (cap[e] == 0)
		{
			// Okay, so the return value of the function needs to be the sum of the ORIGINAL weights
			// in the graph, but we've completely modified those weights since then. How do we get them
			// back?
			//
			// Trick: each original edge e in the graph had a weight w_e. We created a reverse edge
			// for every e, e', which originally had the same weight w_e'=w_e. Every modification to 
			// w_e results in an equal and opposite change in w_e'. By the time w_e is zero (and we're
			// in this if statement clause), w_e' is therefore 2*(original value of w_e).
			//
			// So to get the original value of w_e, we take the current value of w_e' and divide it by 2.
			
			// Look up e's reverse edge (called e' in the above explanation, e2 here)
			EdgeID e2 = R.redge(e);

			// capacity in reverse direction must equal initial weight * 2
			result += cap[e2] / 2;
			
			// Delete the edge in the _original_ graph. This is the only place where G is modified.
			// e is guaranteed to exist in G, despite us iterating over the edges of R, because reasons.
			G.dele(e);
		}
	}

	return result;
}
