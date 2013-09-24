#pragma once

#include "impl_vlog.h"
#include "ct/instance_node.h"
#include "build_spec.h"

using namespace ct;

namespace
{
	Vlog::Port::Dir conv_port_dir(Spec::Interface::Type itype, Spec::Signal::Type stype)
	{
		using namespace ct::Spec;

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

	class : public ImplVerilog::IModuleImpl
	{
		const std::string& get_module_name(ct::P2P::Node* node)
		{
			P2P::InstanceNode* snode = (P2P::InstanceNode*) node;
			Spec::Component* comp_def = Spec::get_component_for_instance(snode->get_instance()->get_name());
			BuildSpec::ComponentImpl* impl = (BuildSpec::ComponentImpl*) comp_def->get_impl();
			return impl->module_name;
		}

		Vlog::Module* implement(ct::P2P::Node* generic_node)
		{
			P2P::InstanceNode* node = (P2P::InstanceNode*) generic_node;
			assert(node->get_type() == P2P::Node::INSTANCE);

			Spec::Component* comp_def = Spec::get_component_for_instance(node->get_instance()->get_name());

			Vlog::Module* result = new Vlog::Module();
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

			return result;
		}

		void parameterize(P2P::Node* generic_node, Vlog::Instance* vinst)
		{
			P2P::InstanceNode* node = (P2P::InstanceNode*) generic_node;
			assert(node->get_type() == P2P::Node::INSTANCE);

			Spec::Instance* inst_def = node->get_instance();
			for (auto& j : inst_def->param_bindings())
			{
				vinst->get_param_binding(j.first)->value() = j.second;
			}
		}

		void get_port_name(P2P::Port* port, P2P::Field* field, ImplVerilog::GPNInfo* result)
		{
			P2P::FieldBinding* fb = port->get_field_binding(field->name);
			Spec::Signal* sig = fb->get_sig_def();
			BuildSpec::SignalImpl* sig_impl = (BuildSpec::SignalImpl*)sig->get_impl();
			
			result->port = sig_impl->signal_name;			
			result->lo = 0;
		}
	} s_impl;

	ImplVerilog::BuiltinReg s_impl_reg ([]
	{
		ImplVerilog::register_impl(P2P::Node::INSTANCE, &s_impl);
	});
}