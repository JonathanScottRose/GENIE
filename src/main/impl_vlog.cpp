#include "build_spec.h"
#include "vlog.h"
#include "impl_vlog.h"

#include "ct/p2p.h"
#include "ct/spec.h"
#include "ct/ct.h"
#include "ct/instance_node.h"
#include "ct/export_node.h"
#include "ct/static_init.h"

using namespace ct;
using namespace ct::Spec;
using namespace ct::P2P;

using namespace ImplVerilog;

namespace
{
	typedef std::unordered_map<P2P::Node::Type, INodeImpl*> INodeImpls;

	// Private
	Vlog::SystemModule* s_cur_top;
	bool s_initialized = false;
	Vlog::ModuleRegistry s_mod_reg;
	INodeImpls s_node_impls;

	// Function prototypes
	void impl_system(P2P::System* sys);

	// Netlist-modification interface for node implementations
	class : public INetlist
	{
		Vlog::ModuleRegistry* get_module_registry()
		{
			return &s_mod_reg;
		}

		Vlog::SystemModule* get_system_module()
		{
			return s_cur_top;
		}
	} s_netlist_iface;

	// Function bodies
	void impl_system(P2P::System* sys)
	{
		s_cur_top->set_name(sys->get_name());

		// Visit each child node and implement it
		for (auto& i : sys->nodes())
		{
			Node* child = i.second;

			INodeImpl* impl = s_node_impls[child->get_type()];
			if (!impl)
				throw Exception("Don't know how to visit node " + child->get_name());
			
			impl->visit(&s_netlist_iface, child);
		}

		// Convert connections into nets and hook them up
		for (Conn* conn : sys->conns())
		{
			P2P::Port* p2psrc_port = conn->get_source();
			Node* p2psrc_node = p2psrc_port->get_parent();

			const Protocol& src_proto = p2psrc_port->get_proto();
			for (Field* f : src_proto.fields())
			{
				INodeImpl* src_impl = s_node_impls[p2psrc_node->get_type()];

				// No constants - nets only
				if (f->is_const)
					continue;

				// Iterate over all sinks pass 1: See if anyone wants to produce a pre-made net
				Vlog::Net* net = src_impl->produce_net(&s_netlist_iface, p2psrc_node, p2psrc_port, f);
				for (P2P::Port* p2psink_port : conn->get_sinks())
				{
					if (net) break;
					Node* p2psink_node = p2psink_port->get_parent();
					INodeImpl* sink_impl = s_node_impls[p2psink_node->get_type()];
					net = sink_impl->produce_net(&s_netlist_iface, p2psink_node, p2psink_port, f);
				}

				// Create an auto-named net if none exists
				if (!net)
				{
					std::string name = p2psrc_node->get_name() + "_" + 
						ImplVerilog::name_for_p2p_port(p2psrc_port, f);
					net = new Vlog::WireNet(name, f->width);
					s_cur_top->add_net(net);
				}

				// Iterate over all sinks pass 2: Connect the net to the sinks and the source
				src_impl->accept_net(&s_netlist_iface, p2psrc_node, p2psrc_port, f, net);
				for (P2P::Port* p2psink_port : conn->get_sinks())
				{
					// Avoid constants
					if (p2psink_port->get_proto().has_field(f->name) &&
						p2psink_port->get_proto().get_field(f->name)->is_const)
						continue;

					Node* p2psink_node = p2psink_port->get_parent();
					INodeImpl* sink_impl = s_node_impls[p2psink_node->get_type()];
					sink_impl->accept_net(&s_netlist_iface, p2psink_node, p2psink_port, f, net);
				}
			}
		}

		// Create constant drivers
		for (auto& i : sys->nodes())
		{
			P2P::Node* node = i.second;
			for (auto& j : node->ports())
			{
				P2P::Port* port = j.second;
				for (Field* f : port->get_proto().fields())
				{
					if (!f->is_const) continue;

					auto c = new Vlog::ConstValue(f->const_val, f->width);
					INodeImpl* impl = s_node_impls[node->get_type()];
					impl->accept_net(&s_netlist_iface, node, port, f, c);
				}
			}
		}
	}
}


Vlog::SystemModule* ImplVerilog::build_top_module(P2P::System* sys)
{
	if (!s_initialized)
	{
		s_initialized = true;

		// Register all node implementations
		ImplVerilog::BuiltinRegHost::execute_inits();
	}

	// Implement system module
	s_cur_top = new Vlog::SystemModule();
	impl_system(sys);

	return s_cur_top;
}


void ImplVerilog::register_impl(P2P::Node::Type type, INodeImpl* impl)
{
	s_node_impls[type] = impl;
}

Vlog::Port::Dir ImplVerilog::conv_port_dir(P2P::Port* port, P2P::Field* field)
{
	bool is_out = port->get_dir() == P2P::Port::OUT;
	is_out ^= field->sense == P2P::Field::REV;
	return is_out ? Vlog::Port::OUT : Vlog::Port::IN;
}

std::string ImplVerilog::name_for_p2p_port(P2P::Port* port, P2P::Field* field)
{
	std::string result = port->get_name();
	
	switch(port->get_type())
	{
		case Port::CLOCK:
		case Port::RESET:
			break;
		default:
			result += "_" + field->name;
	}

	return result;
}

// Generic visitor function for module-like P2P nodes.
// Grab a Module, instantiate it, parameterize it, and add it to the netlist.
void IModuleImpl::visit(INetlist* netlist, P2P::Node* node)
{
	// Get the name of the HDL module produced by this node
	const std::string& mod_name = this->get_module_name(node);

	// Get the named module from the registry
	Vlog::ModuleRegistry* registry = netlist->get_module_registry();
	Vlog::Module* mod = registry->get_module(mod_name);

	// If no Module exists, create one and add it to the registry
	if (!mod)
	{
		mod = this->implement(node, mod_name);	
		registry->add_module(mod);
	}

	// Instantiate the module, add the instance to the system module
	Vlog::Instance* inst = new Vlog::Instance(node->get_name(), mod);
	Vlog::SystemModule* sysmod = netlist->get_system_module();
	sysmod->add_instance(inst);

	// Associate instance with the node
	this->set_inst_for_node(node, inst);

	// Parameterize the instance
	this->parameterize(node, inst);
}

// Module output ports get auto-generated nets, so this returns nothing
Vlog::Net* IModuleImpl::produce_net(INetlist* netlist, P2P::Node* node, P2P::Port* port, 
	P2P::Field* field)
{
	return nullptr;
}

// Connection of nets to the ports of module-like nodes.
void IModuleImpl::accept_net(INetlist* netlist, P2P::Node* node, P2P::Port* port, P2P::Field* field, 
	Vlog::Bindable* net)
{
	// Get the HDL instance corresponding to the node 
	Vlog::Instance* inst = this->get_inst_for_node(node);

	// If it has nowhere to connect to, abort
	if (!port->get_proto().has_field(field->name))
		return;

	// Otherwise, find out which HDL port to bind the net to, and bind it
	GPNInfo ci;
	this->get_port_name(port, field, inst, &ci);
	inst->bind_port(ci.port, net, ci.lo);
}

// Get/set P2P::Node<-->Vlog::Instance association
void IModuleImpl::set_inst_for_node(P2P::Node* node, Vlog::Instance* inst)
{
	InstAspect* a = new InstAspect;
	a->inst = inst;
	node->set_impl("inst", a);
}

Vlog::Instance* IModuleImpl::get_inst_for_node(P2P::Node* node)
{
	auto asp = (InstAspect*)node->get_impl("inst");
	assert(asp);
	return asp->inst;
}

