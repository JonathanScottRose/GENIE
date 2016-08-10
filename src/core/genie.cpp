#include "genie/genie.h"

#include "genie/net_clock.h"
#include "genie/net_reset.h"
#include "genie/net_rvd.h"
#include "genie/net_topo.h"
#include "genie/net_conduit.h"
#include "genie/net_rs.h"
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
	// Register built-in network types
	Network::init();
    NetClock::init();
    NetReset::init();
    NetRS::init();
    NetRVD::init();
    NetTopo::init();
    NetConduit::init();
}
	
HierRoot* genie::get_root()
{
	return &s_hier_root;
}
