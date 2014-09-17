#pragma once

#include "ct/common.h"
#include "ct/sys_spec.h"

namespace ct
{
namespace Spec
{
	struct TopoEdge
	{
		std::string src;
		std::string dest;
		std::vector<Link*> links;
	};

	struct TopoNode
	{
		const std::string& get_name() { return name; }

		std::string name;
		std::string type;
		std::vector<TopoEdge*> ins;
		std::vector<TopoEdge*> outs;
	};

	class TopoGraph
	{
	public:
		typedef std::vector<TopoEdge*> Edges;

		~TopoGraph();
		PROP_DICT(Nodes, node, TopoNode);

		const Edges& edges() { return m_edges; }
		void add_edge(TopoEdge* e);
		void dump_graph(const std::string& filename);

	protected:
		Edges m_edges;
	};
}
}