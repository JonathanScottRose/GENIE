#pragma once

#include "impl_vlog.h"
#include "ct/flow_conv_node.h"

using namespace ct;

namespace
{
	class : public ImplVerilog::IModuleImpl
	{
		const std::string& get_module_name(ct::P2P::Node* node)
		{
			static std::string nm = "ct_field_conv";
			return nm;
		}

		Vlog::Module* implement(ct::P2P::Node* generic_node, const std::string& name)
		{
			Vlog::Module* result = new Vlog::Module();
			result->set_name(name);

			result->add_param(new Vlog::Parameter("WD", result));
			result->add_param(new Vlog::Parameter("WIF", result));
			result->add_param(new Vlog::Parameter("WOF", result));
			result->add_param(new Vlog::Parameter("N_ENTRIES", result));
			result->add_param(new Vlog::Parameter("IF", result));
			result->add_param(new Vlog::Parameter("OF", result));

			result->add_port(new Vlog::Port("clk", result, 1, Vlog::Port::IN));
			result->add_port(new Vlog::Port("reset", result, 1, Vlog::Port::IN));
			result->add_port(new Vlog::Port("i_data", result, "WD", Vlog::Port::IN));
			result->add_port(new Vlog::Port("i_field", result, "WIF", Vlog::Port::IN));
			result->add_port(new Vlog::Port("i_valid", result, 1, Vlog::Port::IN));
			result->add_port(new Vlog::Port("o_ready", result, 1, Vlog::Port::OUT));
			result->add_port(new Vlog::Port("o_valid", result, 1, Vlog::Port::OUT));
			result->add_port(new Vlog::Port("o_data", result, "WD", Vlog::Port::OUT));
			result->add_port(new Vlog::Port("o_field", result, "WOF", Vlog::Port::OUT));
			result->add_port(new Vlog::Port("i_ready", result, 1, Vlog::Port::IN));

			return result;
		}

		void parameterize(P2P::Node* generic_node, Vlog::Instance* vinst)
		{
			using namespace P2P;

			FlowConvNode* node = (FlowConvNode*)generic_node;
			assert(node->get_type() == Node::FLOW_CONV);

			const std::string& in_field_name = node->get_in_field_name();
			const std::string& out_field_name = node->get_out_field_name();

			int data_width = node->get_inport()->get_proto().get_phys_field("data")->width;
			
			vinst->set_param_value("N_ENTRIES", node->get_n_entries());
			vinst->set_param_value("WD", data_width);
			vinst->set_param_value("WIF", node->get_in_field_width());
			vinst->set_param_value("WOF", node->get_out_field_width());

			int flows_param = 0;
			int lpids_param = 0;
			int lpid_width = node->get_in_field_width();
			int flow_width = node->get_out_field_width();

			if (!node->is_to_flow())
				std::swap(lpid_width, flow_width);

			for (Flow* f : node->get_flows())
			{
				flows_param <<= flow_width;
				flows_param |= f->get_id();

				FlowTarget* ft = node->is_to_flow() ? 
					f->get_src() :
					f->get_sink(node->get_user_port());
				
				Spec::Linkpoint* lp = ft->get_linkpoint();
				auto encoding = lp->get_aspect_val<std::string>();

				lpids_param <<= lpid_width;
				lpids_param |= Vlog::parse_constant(encoding);
			}

			if (!node->is_to_flow())
				std::swap(lpids_param, flows_param);

			vinst->set_param_value("IF", lpids_param);
			vinst->set_param_value("OF", flows_param);			
		}

		void get_port_name(P2P::Port* port, P2P::PhysField* pfield, Vlog::Instance* inst, GPNInfo* result)
		{
			using namespace P2P;

			FlowConvNode* node = (FlowConvNode*) port->get_parent();

			if (port->get_name() == "clock")
			{
				result->port = "clk";
				result->lo = 0;
				return;
			}
			else if (port->get_name() == "reset")
			{
				result->port = "reset"; 
				result->lo = 0;
				return;
			}
			
			const Protocol& proto = port->get_proto();

			if (port->get_name() == "in")
			{
				if (pfield->name == "valid") result->port = "i_valid";
				else if (pfield->name == "ready") result->port = "o_ready";
				else if (pfield->name == node->get_in_field_name()) result->port = "i_field";
				else result->port = "i_data";

				result->lo = 0;
			}
			else if (port->get_name() == "out")
			{
				if (pfield->name == "valid") result->port = "o_valid";
				else if (pfield->name == "ready") result->port = "i_ready";
				else if (pfield->name == node->get_out_field_name()) result->port = "o_field";
				else result->port = "o_data";

				result->lo = 0;
			}
		}
	} s_impl;

	ImplVerilog::BuiltinReg s_impl_reg ([]
	{
		ImplVerilog::register_impl(P2P::Node::FLOW_CONV, &s_impl);
	});
}