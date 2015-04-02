#include "genie/genie.h"
#include "genie/static_init.h"

#include "genie/net_clock.h"
#include "genie/net_reset.h"
#include "genie/net_rvd.h"
#include "genie/net_topo.h"
#include "genie/net_conduit.h"
#include "genie/node_split.h"
#include "genie/node_merge.h"

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
	// Register all network types
	Network::init();

	// Register builtin modules
	NodeMerge::init();
	NodeSplit::init();

	{ auto foo = NET_CLOCK ; }
	{ auto foo = NET_RVD; }
	{ auto foo = NET_RESET; }
	{ auto foo = NET_TOPO; }
	{ auto foo = NET_CONDUIT; }
}
	
HierRoot* genie::get_root()
{
	return &s_hier_root;
}
