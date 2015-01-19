#include "genie/genie.h"
#include "genie/static_init.h"

#include "genie/net_clock.h"
#include "genie/net_reset.h"
#include "genie/net_rvd.h"
#include "genie/net_topo.h"
#include "genie/node_split.h"

using namespace genie;

namespace
{
	HierRoot s_hier_root;
}


//
// Public
//

void genie::init()
{
	// Instantiate all the NodeDefs for built-in modules like split and merge
	typedef StaticRegistry<NodeDef> RegisteredBuiltins;
	for (auto& reg_func : RegisteredBuiltins::entries())
	{
		NodeDef* def = reg_func();
		s_hier_root.add_object(def);
	}

	{ auto foo = NET_CLOCK ; }
	{ auto foo = NET_RVD; }
	{ auto foo = NET_RESET; }
	{ auto foo = NET_TOPO; }
	{ SplitNode::prototype(); }
}
	
HierRoot* genie::get_root()
{
	return &s_hier_root;
}
