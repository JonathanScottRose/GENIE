#pragma once

#include "genie/genie.h"

// Attached to a System during Lua-induced system creation.
// Holds a reference to the Lua topology function for the system.
class ATopoFunc : public genie::Aspect
{
public:
	int func_ref;
};

struct FlowOptions
{
	bool force_full_merge = false;
    bool no_topo_opt = false;
};

FlowOptions& flow_options();
void flow_main();
void flow_print_stats(class genie::System* sys);

