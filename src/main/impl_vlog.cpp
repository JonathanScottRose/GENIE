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

namespace
{
	Vlog::Module* s_top;
	Vlog::ModuleRegistry s_mod_reg;

	Vlog::Module* impl_node(P2P::Node* node);
	Vlog::Module* impl_sys_node(P2P::SystemNode* node);
	Vlog::Module* impl_inst_node(P2P::InstanceNode* node);
	Vlog::Port::Dir conv_port_dir(P2P::Port::Dir dir, 
		P2P::Field::Sense sense = P2P::Field::FWD);
	Vlog::Port::Dir conv_port_dir(Spec::Interface::Type itype, Spec::Signal::Type stype);
	std::string concat(const std::string& a, const std::string& b);
	const std::string& get_vlog_port_name(P2P::ClockResetPort* port);
	const std::string& get_vlog_port_name(P2P::DataPort* port, P2P::Field* field);
	void conv_top_level_port(P2P::Port* port, Vlog::SystemModule* module);
	void bind_net_to_port(Vlog::Instance* inst, Vlog::Port* port, Vlog::Net* net);

	void bind_net_to_port(Vlog::Instance* inst, Vlog::Port* port, Vlog::Net* net)
	{
		inst->bind_port(port->get_name(), net);
	}

	void conv_top_level_port(P2P::Port* port, Vlog::SystemModule* module)
	{
		switch (port->get_type())
		{
		case Port::RESET:
		case Port::CLOCK:
			{
				Vlog::Port* vport = new Vlog::Port(port->get_name(), module);
				vport->set_dir(conv_port_dir(P2P::Port::rev_dir(port->get_dir()))); // reverse
				vport->set_width(1);
				module->add_port(vport);

				module->add_net(new Vlog::ExportNet(vport));
			}
			break;

		case Port::DATA:
			{
				P2P::DataPort* dport = (DataPort*)port;
				const P2P::Protocol& proto = dport->get_proto();

				for (P2P::Field* field : proto.fields())
				{
					std::string pname = concat(dport->get_name(), field->name);
					Vlog::Port* vport = new Vlog::Port(pname, module);
					vport->set_width(field->width);
					vport->set_dir(conv_port_dir(P2P::Port::rev_dir(dport->get_dir()), 
						field->sense)); // reverse
					module->add_port(vport);

					module->add_net(new Vlog::ExportNet(vport));
				}
			}
			break;

		default:
			assert(false);
			break;
		}
	}

	const std::string& get_vlog_port_name(P2P::ClockResetPort* port)
	{
		Spec::Signal* sigdef = port->get_sig_def();
		BuildSpec::SignalImpl* impl = (BuildSpec::SignalImpl*) sigdef->get_impl();
		return impl->signal_name;
	}

	const std::string& get_vlog_port_name(P2P::DataPort* port, P2P::Field* field)
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

	Vlog::Port::Dir conv_port_dir(Spec::Interface::Type itype, Spec::Signal::Type stype)
	{
		Vlog::Port::Dir in, out;
		if (stype != Spec::Signal::READY)
		{
			in = Vlog::Port::IN;
			out = Vlog::Port::OUT;
		}
		else
		{
			in = Vlog::Port::OUT;
			out = Vlog::Port::IN;
		}

		switch (itype)
		{
		case Interface::CLOCK_SINK:
		case Interface::RESET_SINK:
		case Interface::RECV:
			return in;
		case Interface::CLOCK_SRC:
		case Interface::RESET_SRC:
		case Interface::SEND:
			return out;
		default:
			assert(false);
			return out;
		}
	}

	Vlog::Module* impl_node(P2P::Node* node)
	{
		switch (node->get_type())
		{
		case P2P::Node::SYSTEM: return impl_sys_node((P2P::SystemNode*)node); break;
		case P2P::Node::INSTANCE: return impl_inst_node((P2P::InstanceNode*)node); break;
		default: assert(false);
		}

		return nullptr;
	}

	Vlog::Module* impl_inst_node(P2P::InstanceNode* node)
	{
		Spec::Component* comp_def = Spec::get_component_for_instance(node->get_instance()->get_name());
		const std::string& comp_name = comp_def->get_name();
		
		// Component already module-ized?
		Vlog::Module* result = s_mod_reg.get_module(comp_name);
		if (result != nullptr)
			return result;

		result = new Vlog::Module();
		result->set_name(comp_def->get_name());
		
		// Convert parameters
		Spec::Instance* inst_def = node->get_instance();
		for (auto& i : inst_def->param_bindings())
		{
			Vlog::Parameter* param = new Vlog::Parameter(i.first, result);
			result->add_param(param);
			// derp: add default parameter value?
		}

		// Convert ports
		for (auto& i : comp_def->interfaces())
		{
			Spec::Interface* iface = i.second;
			const std::string& ifacename = iface->get_name();

			for (Spec::Signal* sig : iface->signals())
			{
				BuildSpec::SignalImpl* impl = (BuildSpec::SignalImpl*)sig->get_impl();

				Vlog::Port* port = new Vlog::Port(impl->signal_name, result);
				port->set_dir(conv_port_dir(iface->get_type(), sig->get_type()));
				port->width() = sig->get_width();
				result->add_port(port);
			}
		}

		s_mod_reg.add_module(result);
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

			// Copy parameter bindings... uh oh
			if (child->get_type() == P2P::Node::INSTANCE)
			{
				P2P::InstanceNode* child2 = (P2P::InstanceNode*)child;
				Spec::Instance* inst_def = child2->get_instance();
				for (auto& j : inst_def->param_bindings())
				{
					child_inst->get_param_binding(j.first)->value() = j.second;
				}
			}
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
			
			switch (p2psrc->get_type())
				{
				case Port::RESET:
				case Port::CLOCK:
					{
						Vlog::Net* net;

						if (internal)
						{
							Vlog::WireNet* wnet = new Vlog::WireNet();
							net = wnet;

							wnet->set_name(concat(p2psrc_node->get_name(), p2psrc->get_name()));
							wnet->set_width(1);

							Vlog::Instance* src_inst = result->get_instance(p2psrc_node->get_name());
							Vlog::Port* src_port = src_inst->get_module()->get_port(
								get_vlog_port_name((ClockResetPort*)p2psrc));

							bind_net_to_port(src_inst, src_port, wnet);							

							result->add_net(wnet);
						}
						else
						{
							net = result->get_net(p2psrc->get_name());
						}

						for (P2P::Port* p2pdest : conn->get_sinks())
						{
							Node* p2pdest_node = p2pdest->get_parent();
							Vlog::Instance* dest_inst = result->get_instance(p2pdest_node->get_name());
							Vlog::Port* dest_port = dest_inst->get_module()->get_port(
								get_vlog_port_name((ClockResetPort*)p2pdest));
							bind_net_to_port(dest_inst, dest_port, net);							
						}						
					}
					break;

				case Port::DATA:
				{
					const Protocol& proto = ((DataPort*)p2psrc)->get_proto();

					for (Field* f : proto.fields())
					{
						Vlog::Net* net;

						// Create/get net
						if (src_is_top)
						{
							net = result->get_net(concat(p2psrc->get_name(), f->name));
						}
						else if (sink_is_top)
						{
							net = result->get_net(concat(p2psink_0->get_name(), f->name));
						}
						else
						{
							Vlog::WireNet* wnet = new Vlog::WireNet();
							wnet->set_name(concat(p2psrc_node->get_name(), concat(p2psrc->get_name(),
								f->name)));
							wnet->set_width(f->width);
							result->add_net(wnet);
							net = wnet;
						}

						// Bind source
						if (!src_is_top)
						{
							Vlog::Instance* src_inst = result->get_instance(p2psrc_node->get_name());
							Vlog::Port* src_port = src_inst->get_module()->get_port(
								get_vlog_port_name((P2P::DataPort*)p2psrc, f));
							bind_net_to_port(src_inst, src_port, net);
						}

						// Bind sink
						if (!sink_is_top)
						{
							Vlog::Instance* sink_inst = result->get_instance(p2psink_0_node->get_name());
							Vlog::Port* sink_port = sink_inst->get_module()->get_port(
								get_vlog_port_name((P2P::DataPort*)p2psink_0, f));
							bind_net_to_port(sink_inst, sink_port, net);
						}
					}
				}
				break;

			default:
				assert(false);
				break;
			}
		}

		// Register module
		s_mod_reg.add_module(result);

		return result;
	}

	
}

void ImplVerilog::go()
{
	s_top = impl_node(ct::get_root_node());
}

Vlog::Module* ImplVerilog::get_top_module()
{
	return s_top;
}

Vlog::ModuleRegistry* ImplVerilog::get_module_registry()
{
	return &s_mod_reg;
}