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
	struct TopoOptState;

	TopoOptState* init(NodeSystem* sys, flow::FlowStateOuter& fstate);
	void iter_newbase(TopoOptState*, NodeSystem*);
	NodeSystem* iter_next(TopoOptState*);
	void cleanup(TopoOptState*);
}
}
}