#pragma once

#include "genie/hierarchy.h"
#include "genie/structure.h"
#include "genie/networks.h"
#include "genie/static_init.h"

namespace genie
{
	// Registration of builtins (temporary)
	template<class T> using RegisterBuiltin = StaticRegistryEntry<T, NodeDef>;

	// Initialize library (temporary)
	void init();

	// Hierarchy root
	HierRoot* get_root();
}

