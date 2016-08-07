#include <algorithm>
#include <cassert>
#include "genie/graph.h"

using namespace genie;
using namespace genie::graphs;

// Finds a shortest path in graph g from vertex 'src' to vertex 'dest.
// The function returns true if a path was found.
// The path's vertices are stored in v_out, if it is not null.
// The path's edges are stored in e_out, if it is not null.
// Edge weights (distances between vertices) are provided in 'distances'.
// If 'distances' is null, all edge weights are assumed to be 1.
bool graphs::dijkstra(const Graph& g, VertexID src, VertexID dest, 
	const EAttr<int>* distances, VList* v_out, EList* e_out)
{
	// Initialize outputs
	bool result = false;

	if (v_out)
		v_out->clear();

	if (e_out)
		e_out->clear();

	// For each vertex, keep track of the shortest path found so far from the source,
	// and the previous vertex on said path.
	struct Entry
	{
		int shortest = std::numeric_limits<int>::max();
		VertexID prev = INVALID_V;
	};

	VAttr<Entry> entries;

	// Shortest path from src to src initialized to 0
	entries[src].shortest = 0;

	// Visitation list
	VList to_visit{src};

	while (!to_visit.empty())
	{
		// Find the vertex with the lowest cost to visit, and remove it from the list
		auto cur_iter = std::min_element(to_visit.begin(), to_visit.end(), 
			[&](VertexID a, VertexID b)
		{
			return entries[a].shortest < entries[b].shortest;
		});

		VertexID cur = *cur_iter;
		to_visit.erase(cur_iter);

		// If reached destination, then done
		if (cur == dest)
		{
			result = true;
			break;
		}

		int cur_cost = entries[cur].shortest;

		// Iterate through directed outbound neighbours
		auto edges = g.dir_edges(cur);
		for (auto edge : edges)
		{
			// Neighbour vertex and its information
			int neigh = g.sink_vert(edge);
			auto& neigh_entry = entries[neigh];

			// Incremental cost of going to 'neigh' from 'cur'
			int edge_cost = distances? distances->at(edge) : 1;

			// Total cost of going from 'src' to 'neigh' through 'cur'
			int new_cost = cur_cost + edge_cost;

			// Branch out to it if it's unvisited
			if (neigh_entry.prev == INVALID_V)
				to_visit.push_back(neigh);

			// Found new best path to 'neigh' through 'cur'
			if (new_cost < neigh_entry.shortest)
			{
				neigh_entry.shortest = new_cost;
				neigh_entry.prev = cur;
			}
		}
	}

	// No path found? Early return
	if (!result)
		return false;

	// Populate result
	VertexID cur = dest;
	while (cur != src)
	{
		if (v_out) v_out->push_back(cur);
		VertexID prev = entries[cur].prev;

		if (e_out)
		{
			EdgeID e = g.edge(prev, cur);
			assert(e != INVALID_E);
			e_out->push_back(e);
		}

		cur = prev;
	}

	if (v_out) std::reverse(v_out->begin(), v_out->end());
	if (e_out) std::reverse(e_out->begin(), e_out->end());

	return true;
}