#include "pch.h"
#include "graph.h"

using namespace genie::impl;

// Given a graph g, find the number of connected components.
// Two vertices are in the same component if they are connected by an edge.
// Vertices (and edges) are labeled with an integer component ID (compid).
// This returns the number of components, along with vertex->compid and edge->compid mappings.

int graph::connected_comp(Graph& g, V2Attr<int>* vcolor, E2Attr<int>* ecolor)
{
	int colors = 0;

	bool local_vcolor = !vcolor;
	if (local_vcolor)
		vcolor = new V2Attr<int>;

	// Go through all vertices, trying to find ones that haven't been colored yet
	for (auto v : g.verts())
	{
		// Only visit uncolored vertices
		if (vcolor->count(v))
			continue;

		// This vertex and everything connected to it will be painted as 'colors'
		std::stack<VertexID> to_visit({v});

		while (!to_visit.empty())
		{
			VertexID u = to_visit.top();
			to_visit.pop();

			// Only spread to uncolored vertices
			if (vcolor->count(u))
				continue;

			// Paint vertex
			vcolor->emplace(u, colors);

			// Spread. 
			for (auto e : g.edges(u))
			{
				VertexID neigh = g.otherv(e, u);
				to_visit.push(neigh);

				// Also color edges if requested by the caller
				if (ecolor) ecolor->emplace(e, colors);
			}
		}

		// We've spread as far as we can. Any further vertices will be a different color/component.
		colors++;
	}

	if (local_vcolor)
		delete vcolor;

	return colors;
}