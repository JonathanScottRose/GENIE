#pragma once

#include "genie/genie.h"

// Attached to a System during Lua-induced system creation.
// Holds a reference to the Lua topology function for the system.
class ATopoFunc : public genie::Aspect
{
public:
	int func_ref;
};

void flow_main();

