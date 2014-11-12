#include "genie/genie.h"

using namespace genie;

namespace
{
	HierRoot s_hier_root;
}


//
// Public
//

	
HierRoot* genie::get_root()
{
	return &s_hier_root;
}

