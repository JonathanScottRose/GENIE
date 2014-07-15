#include "ct/topo_spec.h"

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

void TopoGraph::dump_graph(const std::string& filename)
{
	std::ofstream out(filename);

	out << "digraph netlist {" << std::endl;

	for (auto& edge : m_edges)
	{
		out << '"' << edge->src << "\" -> \"" << edge->dest <<
			"\" [label=" << edge->links.size() << "];" << std::endl;
	}

	out << "}" << std::endl;
}