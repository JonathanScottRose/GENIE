#include "topo_spec.h"

using namespace ct;
using namespace Spec;

TopoGraph::~TopoGraph()
{
	Util::delete_all(m_edges);
	Util::delete_all_2(m_nodes);
}

void TopoGraph::add_edge(TopoEdge* e)
{
	m_edges.push_back(e);
}
