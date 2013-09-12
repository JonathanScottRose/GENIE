#pragma once

#include "vlog.h"

namespace ImplVerilog
{
	void go();
	Vlog::Module* get_top_module();
	Vlog::ModuleRegistry* get_module_registry();
}
