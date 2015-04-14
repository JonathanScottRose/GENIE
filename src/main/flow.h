#pragma once

#include "genie/genie.h"

// Attached to a System during Lua-induced system creation.
// Holds a reference to the Lua topology function for the system.
class ATopoFunc : public genie::Aspect
{
public:
	int func_ref;
};

// RS->TOPO
void flow_refine_rs(genie::System* sys);

// TOPO->RVD
void flow_refine_topo(genie::System* sys);

// RVD refinement stages
void flow_process_rvd(genie::System* sys);
