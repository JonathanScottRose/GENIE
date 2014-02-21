#pragma once

#include "impl_vlog.h"
#include "ct/instance_node.h"

using namespace ct;

namespace
{
	Vlog::Port::Dir conv_port_dir(Spec::Interface::Type itype, Spec::Signal::Sense ssense)
	{
		using namespace ct::Spec;

		bool is_out = ssense == Signal::FWD;		

		switch (itype)
		{
		case Interface::CLOCK_SINK:
		case Interface::RESET_SINK:
		case Interface::RECV:
			is_out = !is_out;
			break;
		}

		return is_out? Vlog::Port::OUT : Vlog::Port::IN;
	}

	class : public ImplVerilog::IModuleImpl
	{
		const std::string& get_module_name(ct::P2P::Node* node)
		{
			P2P::InstanceNode* snode = (P2P::InstanceNode*) node;
			Spec::Component* comp_def = Spec::get_component(snode->get_instance()->get_component());
			return comp_def->get_aspect_val<std::string>();
		}

		Vlog::Module* implement(ct::P2P::Node* generic_node, const std::string& name)
		{
			P2P::InstanceNode* node = (P2P::InstanceNode*) generic_node;
			assert(node->get_type() == P2P::Node::INSTANCE);

			Spec::Component* comp_def = Spec::get_component(node->get_instance()->get_component());

			Vlog::Module* result = new Vlog::Module();
			result->set_name(name);
		
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
					std::string signal_name = sig->get_aspect_val<std::string>();
					Vlog::Port* port = new Vlog::Port(signal_name, result);
					port->set_dir(conv_port_dir(iface->get_type(), sig->get_sense()));
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

		void get_port_name(P2P::Port* port, P2P::PhysField* pfield, Vlog::Instance* inst, GPNInfo* result)
		{
			auto iface = port->get_aspect<Spec::Interface>("iface_def");
			Spec::Signal* sig = iface->get_signal(pfield->name);
			
			result->port = sig->get_aspect_val<std::string>();
			result->lo = 0;
		}
	} s_impl;

	ImplVerilog::BuiltinReg s_impl_reg ([]
	{
		ImplVerilog::register_impl(P2P::Node::INSTANCE, &s_impl);
	});
}