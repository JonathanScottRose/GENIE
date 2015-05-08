#pragma once

#include "genie/hierarchy.h"
#include "genie/structure.h"
#include "genie/networks.h"
#include "genie/static_init.h"

namespace genie
{
	// Initialize library
	void init();

	// Hierarchy root
	HierRoot* get_root();
}

