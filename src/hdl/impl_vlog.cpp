/*
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
	typedef std::unordered_map<P2P::Node::Type, INodeImpl*, std::hash<int>> INodeImpls;

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

				for (auto& i : sink_proto.fields())
				{
					Field* sink_f = i.second;
					FieldState* sink_fs = sink_proto.get_field_state(i.first);
					PhysField* sink_pf = sink_proto.get_phys_field(sink_fs->phys_field);

					if (sink_pf->sense == PhysField::REV || 
						sink_fs->is_const || !src_proto.has_field(i.first))
						continue;

					Field* src_f = src_proto.get_field(i.first);
					FieldState* src_fs = src_proto.get_field_state(i.first);
					PhysField* src_pf = src_proto.get_phys_field(src_fs->phys_field);

					// The net used for the connection - try the sink first to see if it produces
					// a net (if it's an export net for example)
					Vlog::Net* net = sink_impl->produce_net(&s_netlist_iface, p2psink_node,
						p2psink_port, sink_f, 0);

					if (net)
					{
						src_impl->accept_net(&s_netlist_iface, p2psrc_node, p2psrc_port, src_f, net);
					}
					else
					{
						int net_lo;
						net = src_impl->produce_net(&s_netlist_iface, p2psrc_node, p2psrc_port, src_f,
							&net_lo);
						sink_impl->accept_net(&s_netlist_iface, p2psink_node, p2psink_port, sink_f,
							net, net_lo);
					}
				}

				for (auto& i : src_proto.fields())
				{
					Field* src_f = i.second;
					FieldState* src_fs = src_proto.get_field_state(i.first);
					PhysField* src_pf = src_proto.get_phys_field(src_fs->phys_field);

					if (src_pf->sense == PhysField::FWD ||
						src_fs->is_const || !sink_proto.has_field(i.first))
						continue;

					Field* sink_f = sink_proto.get_field(i.first);
					FieldState* sink_fs = sink_proto.get_field_state(i.first);
					PhysField* sink_pf = sink_proto.get_phys_field(sink_fs->phys_field);

					Vlog::Net* net = src_impl->produce_net(&s_netlist_iface, p2psrc_node,
						p2psrc_port, src_f, 0);

					if (net)
					{
						sink_impl->accept_net(&s_netlist_iface, p2psink_node, p2psink_port, sink_f, net);
					}
					else
					{
						int net_lo;
						net = sink_impl->produce_net(&s_netlist_iface, p2psink_node, p2psink_port, sink_f,
							&net_lo);
						src_impl->accept_net(&s_netlist_iface, p2psrc_node, p2psrc_port, src_f,
							net, net_lo);
					}
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

// Output ports of module-like nodes produce nets, but input ports don't.
Vlog::Net* IModuleImpl::produce_net(INetlist* netlist, P2P::Node* node, P2P::Port* port, 
	P2P::Field* field, int* net_lo)
{
	// Determine field's physical containment/location
	FieldState* fs = port->get_proto().get_field_state(field->name);
	PhysField* pfield = port->get_proto().get_phys_field(fs->phys_field);

	if ((port->get_dir() == P2P::Port::IN) != (pfield->sense == P2P::PhysField::REV))
		return nullptr;

	// Get the HDL instance corresponding to the node 
	Vlog::Instance* inst = this->get_inst_for_node(node);

	// P2PPort + Physfield --> HDL Port
	GPNInfo ci;
	this->get_port_name(port, pfield, inst, &ci);

	// find/create a net which is named instname_portname, and is as wide as the entire (output) port
	std::string netname = inst->get_name() + "_" + ci.port;
	Vlog::Net* result = netlist->get_system_module()->get_net(netname);
	if (!result)
	{
		result = new Vlog::WireNet(netname, inst->get_port_state(ci.port)->get_width());
		netlist->get_system_module()->add_net(result);
		inst->bind_port(ci.port, result); // also bind!
	}

	if (net_lo) *net_lo = ci.lo + fs->phys_field_lo;
	return result;
}

// Connection of nets to the ports of module-like nodes.
// Binds a given net to a subset of a port
void IModuleImpl::accept_net(INetlist* netlist, P2P::Node* node, P2P::Port* port, 
	P2P::Field* field, Vlog::Bindable* net, int net_lo)
{
	// Get the HDL instance corresponding to the node 
	Vlog::Instance* inst = this->get_inst_for_node(node);

	// Determine field's physical containment/location
	FieldState* fs = port->get_proto().get_field_state(field->name);
	PhysField* pfield = port->get_proto().get_phys_field(fs->phys_field);

	// Find out which HDL port to bind the net to, and bind it
	GPNInfo ci;
	this->get_port_name(port, pfield, inst, &ci);

	// Bind only as many bits as given/available
	int width = std::min(net->get_width(), field->width);

	inst->bind_port(ci.port, net, width, ci.lo + fs->phys_field_lo, net_lo);
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

*/