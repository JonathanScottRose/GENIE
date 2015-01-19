#pragma once

#include "genie/structure.h"

namespace genie
{
	class SplitNode : public Node
	{
	public:
		static NodeDef* prototype();

		SplitNode();
		SplitNode(const std::string& name, System* parent = nullptr);
		~SplitNode();

		int get_n_outputs() const;
		Port* get_input() const;
		Port* get_output(int idx) const;

	protected:
		Port** m_outputs;
		bool m_topo_finalized;
		int m_n_outputs;
	};
}