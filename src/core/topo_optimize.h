#pragma once

#include <unordered_map>
#include "network.h"
#include "flow.h"

namespace genie
{
namespace impl
{
namespace topo_opt
{
	using ContentionMap = std::unordered_map<LinkID, unsigned>;
	/*
	struct State
	{
		ContentionMap xbar_contention;
		std::unordered_map<NodeMerge*, std::vector<flow::TransmissionID>> merge_xmis;
	};

	void init_state(NodeSystem* sys, 

	ContentionMap measure_initial_contention(NodeSystem* sys, flow::FlowStateOuter& fstate);
	*/
}
}
}