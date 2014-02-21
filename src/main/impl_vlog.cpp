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
			INodeImpl* src_impl = s_node_impls[p2psrc_node->get_type()];
			
			// Connect each sink to the source
			for (P2P::Port* p2psink_port : conn->get_sinks())
			{
				Node* p2psink_node = p2psink_port->get_parent();
				const Protocol& sink_proto = p2psink_port->get_proto();
				INodeImpl* sink_impl = s_node_impls[p2psink_node->get_type()];

				// Bind each logical field
				for (auto& i : sink_proto.fields())
				{
					Field* f = i.second;

					// Query the source about this field. Does it even have it? If not, skip.
					FieldState* src_fs = src_proto.get_field_state(i.first);
					if (!src_fs)
						continue;

					// Source's physical encapsulation of the field
					PhysField* src_pf = src_proto.get_phys_field(src_fs->phys_field);

					// Query the state of the field at the sink. Skip processing of constant drivers
					// until later.
					FieldState* sink_fs = sink_proto.get_field_state(i.first);
					if (sink_fs->is_const)
						continue;

					// Sink's physical encapsulation of the field
					PhysField* sink_pf = sink_proto.get_phys_field(sink_fs->phys_field);
					
					// Try to obtain an existing net from either the source or the sink
					Vlog::Net* net = src_impl->produce_net(&s_netlist_iface, p2psrc_node, p2psrc_port,
						sink_pf);

					if (!net)
					{
						net = sink_impl->produce_net(&s_netlist_iface, p2psink_node, p2psink_port, 
							sink_pf);
					}

					// Try an existing auto-generated wire net
					std::string net_name = p2psrc_node->get_name() + "_" +
						ImplVerilog::name_for_p2p_port(p2psrc_port, src_pf);

					net = s_cur_top->get_net(net_name);

					// Create it if it doesn't exist yet, and bind it to the source
					if (!net)
					{
						net = new Vlog::WireNet(net_name, src_pf->width);
						s_cur_top->add_net(net);

						src_impl->accept_net(&s_netlist_iface, p2psrc_node, p2psrc_port, src_pf, 
							nullptr, net);
					}

					// Bind the net to (part of) the sink's physfield (the part that contains the
					// field we're interested in)
					sink_impl->accept_net(&s_netlist_iface, p2psink_node, p2psink_port, sink_pf, 
						f, net, src_fs->phys_field_lo);
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
				const Protocol& proto = port->get_proto();
				for (auto& j : port->get_proto().fields())
				{
					Field* f = j.second;
					FieldState* fs = proto.get_field_state(j.first);
					PhysField* pf = proto.get_phys_field(fs->phys_field);

					if (!fs->is_const) continue;

					auto c = new Vlog::ConstValue(fs->const_value, f->width);
					INodeImpl* impl = s_node_impls[node->get_type()];
					impl->accept_net(&s_netlist_iface, node, port, pf, f, c);
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

Vlog::Port::Dir ImplVerilog::conv_port_dir(P2P::Port* port, P2P::PhysField* pf)
{
	bool is_out = port->get_dir() == P2P::Port::OUT;
	is_out ^= pf->sense == P2P::PhysField::REV;
	return is_out ? Vlog::Port::OUT : Vlog::Port::IN;
}

std::string ImplVerilog::name_for_p2p_port(P2P::Port* port, P2P::PhysField* pf)
{
	std::string result = port->get_name();
	
	switch(port->get_type())
	{
		case Port::CLOCK:
		case Port::RESET:
			break;
		default:
			result += "_" + pf->name;
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
	P2P::PhysField* field)
{
	return nullptr;
}

// Connection of nets to the ports of module-like nodes.
void IModuleImpl::accept_net(INetlist* netlist, P2P::Node* node, P2P::Port* port, P2P::PhysField* pfield,
	P2P::Field* field, Vlog::Bindable* net, int net_lo)
{
	// Get the HDL instance corresponding to the node 
	Vlog::Instance* inst = this->get_inst_for_node(node);

	// Find out which HDL port to bind the net to, and bind it
	GPNInfo ci;
	this->get_port_name(port, pfield, inst, &ci);

	if (!field)
	{
		// bind to the entire physfield
		inst->bind_port(ci.port, net, pfield->width, ci.lo, net_lo);
	}
	else
	{
		// Bind to just a part of the physfield
		FieldState* fs = port->get_proto().get_field_state(field->name);
		inst->bind_port(ci.port, net, field->width, ci.lo + fs->phys_field_lo, net_lo);
	}
}

// Get/set P2P::Node<-->Vlog::Instance association
void IModuleImpl::set_inst_for_node(P2P::Node* node, Vlog::Instance* inst)
{
	node->set_aspect("inst", inst);
}

Vlog::Instance* IModuleImpl::get_inst_for_node(P2P::Node* node)
{
	auto result = node->get_aspect<Vlog::Instance>("inst");
	assert(result);
	return result;
}

