#include "build_spec.h"
#include "vlog.h"
#include "impl_vlog.h"

#include "ct/p2p.h"
#include "ct/spec.h"
#include "ct/ct.h"
#include "ct/instance_node.h"
#include "ct/static_init.h"

using namespace ct;
using namespace ct::Spec;
using namespace ct::P2P;

using namespace ImplVerilog;

namespace
{
	typedef std::unordered_map<P2P::Node::Type, ModuleImpl*> ModuleImpls;

	Vlog::Module* s_top;
	Vlog::ModuleRegistry s_mod_reg;
	ModuleImpls s_module_impls;

	Vlog::Module* impl_node(P2P::Node* node);
	Vlog::Module* impl_sys_node(P2P::SystemNode* node);
	Vlog::Port::Dir conv_port_dir(P2P::Port::Dir dir, 
		P2P::Field::Sense sense = P2P::Field::FWD);
	std::string concat(const std::string& a, const std::string& b);
	const std::string& get_vlog_port_name(P2P::Port* port, P2P::Field* field);
	void conv_top_level_port(P2P::Port* port, Vlog::SystemModule* module);
	void bind_net_to_port(Vlog::Instance* inst, Vlog::Port* port, Vlog::Net* net);

	void bind_net_to_port(Vlog::Instance* inst, Vlog::Port* port, Vlog::Net* net)
	{
		inst->bind_port(port->get_name(), net);
	}

	void conv_top_level_port(P2P::Port* port, Vlog::SystemModule* module)
	{
		const P2P::Protocol& proto = port->get_proto();

		// haaack
		bool single_field = proto.fields().size() == 1;

		for (P2P::Field* field : proto.fields())
		{
			std::string pname = single_field? port->get_name() : 
				concat(port->get_name(), field->name);
			Vlog::Port* vport = new Vlog::Port(pname, module);
			vport->set_width(field->width);
			vport->set_dir(conv_port_dir(P2P::Port::rev_dir(port->get_dir()), 
				field->sense)); // reverse
			module->add_port(vport);

			module->add_net(new Vlog::ExportNet(vport));
		}	
	}

	const std::string& get_vlog_port_name(P2P::Port* port, P2P::Field* field)
	{
		Spec::Signal* sigdef = port->get_field_binding(field->name)->get_sig_def();
		BuildSpec::SignalImpl* impl = (BuildSpec::SignalImpl*) sigdef->get_impl();
		return impl->signal_name;
	}

	std::string concat(const std::string& a, const std::string& b)
	{
		return a + "_" + b;
	}

	Vlog::Port::Dir conv_port_dir(P2P::Port::Dir dir, P2P::Field::Sense sense)
	{
		P2P::Port::Dir dir2 = sense == P2P::Field::FWD ? dir :
			P2P::Port::rev_dir(dir);

		switch (dir2)
		{
		case P2P::Port::IN: return Vlog::Port::IN;
		case P2P::Port::OUT: return Vlog::Port::OUT;
		default: assert(false); return Vlog::Port::INOUT;
		}
	}

	
	Vlog::Module* impl_node(P2P::Node* node)
	{
		if (node->get_type() == P2P::Node::SYSTEM)
			return impl_sys_node((P2P::SystemNode*) node);

		ModuleImpl* impl = s_module_impls[node->get_type()];
		const std::string& mod_name = impl->get_module_name(node);

		Vlog::Module* result = s_mod_reg.get_module(mod_name);
		if (result == nullptr)
		{
			result = impl->implement(node);
			s_mod_reg.add_module(result);
		}

		return result;
	}


	Vlog::Module* impl_sys_node(P2P::SystemNode* node)
	{
		Vlog::SystemModule* result = new Vlog::SystemModule();
		result->set_name(node->get_name());

		// Instantiate child nodes as instances
		for (auto& i : node->nodes())
		{
			Node* child = i.second;

			// Get the module associated with the child node (could be recursive!)
			Vlog::Module* child_mod = impl_node(child);

			// Create instance and add to module
			Vlog::Instance* child_inst = new Vlog::Instance(child->get_name(), child_mod);
			result->add_instance(child_inst);

			// Parameterize instance
			ModuleImpl* impl = s_module_impls[child->get_type()];
			assert(impl);
			impl->parameterize(child, child_inst);
		}

		// Convert top-level Ports into top-level ports
		for (auto& i : node->ports())
		{
			conv_top_level_port(i.second, result);
		}

		// Convert connections into nets and hook them up
		for (Conn* conn : node->conns())
		{
			P2P::Port* p2psrc = conn->get_source();
			Node* p2psrc_node = p2psrc->get_parent();
			bool src_is_top = p2psrc_node == node;
			
			// todo: some sinks are internal, some are not. right now they must all be the same kind.
			P2P::Port* p2psink_0 = conn->get_sinks().front();
			Node* p2psink_0_node = p2psink_0->get_parent();
			bool sink_is_top = p2psink_0_node == node;

			bool internal = !src_is_top && !sink_is_top;
			assert(!src_is_top || !sink_is_top);		

			const Protocol& proto = ((DataPort*)p2psrc)->get_proto();

			bool single_field = proto.fields().size() == 1;

			for (Field* f : proto.fields())
			{
				Vlog::Net* net;

				// Create/get net
				if (src_is_top)
				{
					std::string netname = single_field? p2psrc->get_name() :
						concat(p2psrc->get_name(), f->name);
					net = result->get_net(netname);
				}
				else if (sink_is_top)
				{
					std::string netname = single_field? p2psink_0->get_name() :
						concat(p2psink_0->get_name(), f->name);
					net = result->get_net(netname);
				}
				else
				{
					Vlog::WireNet* wnet = new Vlog::WireNet();
					std::string subname = single_field? p2psrc->get_name() :
						concat(p2psrc->get_name(), f->name);
					wnet->set_name(concat(p2psrc_node->get_name(), subname));
					wnet->set_width(f->width);
					result->add_net(wnet);
					net = wnet;
				}

				// Bind source
				if (!src_is_top)
				{
					Vlog::Instance* src_inst = result->get_instance(p2psrc_node->get_name());
					Vlog::Port* src_port = src_inst->get_module()->get_port(
						get_vlog_port_name(p2psrc, f));
					bind_net_to_port(src_inst, src_port, net);
				}

				// Bind sink
				if (!sink_is_top)
				{
					for (P2P::Port* p2psink : conn->get_sinks())
					{
						Node* p2psink_node = p2psink->get_parent();
						Vlog::Instance* sink_inst = result->get_instance(p2psink_node->get_name());
						Vlog::Port* sink_port = sink_inst->get_module()->get_port(
							get_vlog_port_name(p2psink, f));
						bind_net_to_port(sink_inst, sink_port, net);
					}
				}
			}
		}

		return result;
	}
}

void ImplVerilog::go()
{
	// Register all modules
	ImplVerilog::BuiltinRegHost::execute_inits();

	// Implement system module
	s_top = impl_node(ct::get_root_node());
}

Vlog::Module* ImplVerilog::get_top_module()
{
	return s_top;
}

void ImplVerilog::register_impl(P2P::Node::Type type, ModuleImpl* impl)
{
	s_module_impls[type] = impl;
}

