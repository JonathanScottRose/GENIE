#pragma once

#include "vlog.h"
#include "ct/static_init.h"
#include "ct/p2p.h"

using namespace ct;

namespace ImplVerilog
{
	// Lets module implementations register themselves
	DEFINE_INIT_HOST(builtin_reg, BuiltinRegHost, BuiltinReg);

	class INetlist
	{
	public:
		virtual Vlog::ModuleRegistry* get_module_registry() = 0;
		virtual Vlog::SystemModule* get_system_module() = 0;
	};

	// Interface for registering node implementations
	class INodeImpl
	{
	public:
		virtual void visit(INetlist*, P2P::Node*) = 0;
		virtual Vlog::Net* produce_net(INetlist*, P2P::Node*, P2P::Port*, P2P::PhysField*) = 0;
		virtual void accept_net(INetlist*, P2P::Node*, P2P::Port*, P2P::PhysField*, P2P::Field*, 
			Vlog::Bindable*, int net_lo = 0) = 0;
	};

	// This is specifically for nodes representing module instances
	class IModuleImpl : public INodeImpl
	{
	public:
		// Used as return value for get_port_name()
		struct GPNInfo
		{
			std::string port;
			int lo;
		};

		void visit(INetlist*, P2P::Node*);
		Vlog::Net* produce_net(INetlist*, P2P::Node*, P2P::Port*, P2P::PhysField*);
		void accept_net(INetlist*, P2P::Node*, P2P::Port*, P2P::PhysField*, P2P::Field*, 
			Vlog::Bindable*, int);

	protected:
		void set_inst_for_node(P2P::Node*, Vlog::Instance*);
		Vlog::Instance* get_inst_for_node(P2P::Node*);

		virtual const std::string& get_module_name(ct::P2P::Node* node) = 0;
		virtual Vlog::Module* implement(P2P::Node*, const std::string&) = 0;
		virtual void parameterize(P2P::Node*, Vlog::Instance*) = 0;
		virtual void get_port_name(P2P::Port*, P2P::PhysField*, Vlog::Instance*, GPNInfo*) = 0;
	};

	void init();
	Vlog::SystemModule* build_top_module(P2P::System* sys);
	void register_impl(P2P::Node::Type type, INodeImpl* entry);

	// Utility
	Vlog::Port::Dir conv_port_dir(P2P::Port*, P2P::PhysField*);
	std::string name_for_p2p_port(P2P::Port*, P2P::PhysField*);
}
