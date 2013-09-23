#pragma once

#include "vlog.h"
#include "ct/static_init.h"
#include "ct/p2p.h"

namespace ImplVerilog
{
	DEFINE_INIT_HOST(builtin_reg, BuiltinRegHost, BuiltinReg);

	// Interface for registering node implementations
	class ModuleImpl
	{
	public:
		virtual const std::string& get_module_name(ct::P2P::Node*) = 0;
		virtual void parameterize(ct::P2P::Node*, Vlog::Instance*) = 0;
		virtual Vlog::Module* implement(ct::P2P::Node*) = 0;
	};

	void go();
	Vlog::Module* get_top_module();
	void register_impl(ct::P2P::Node::Type type, ModuleImpl* entry);
}
