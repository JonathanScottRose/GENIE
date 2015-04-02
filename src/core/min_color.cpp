#include <stack>
#include "genie/graph.h"

using namespace genie;
using namespace graphs;

// Given a graph g, find the number of connected components.
// Two vertices are in the same component if they are connected by an edge.
// Vertices (and edges) are labeled with an integer component ID (compid).
// This returns the number of components, along with vertex->compid and edge->compid mappings.

int graphs::connected_comp(Graph& g, VAttr<int>* vcolor, EAttr<int>* ecolor)
{
	int colors = 0;

	bool local_vcolor = !vcolor;
	if (local_vcolor)
		vcolor = new VAttr<int>;

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
			VertexID v = to_visit.top();
			to_visit.pop();

			// Only spread to uncolored vertices
			if (vcolor->count(v))
				continue;

			// Paint vertex
			vcolor->emplace(v, colors);

			// Spread. Color edges if requested.
			for (auto e : g.edges(v))
			{
				VertexID neigh = g.otherv(e, v);
				to_visit.push(neigh);
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