#pragma once

#include "impl_vlog.h"
#include "ct/merge_node.h"

using namespace ct;

namespace
{
	class : public ImplVerilog::IModuleImpl
	{
		const std::string& get_module_name(ct::P2P::Node* node)
		{
			static std::string nm = "ct_merge";
			return nm;
		}

		Vlog::Module* implement(ct::P2P::Node* generic_node, const std::string& name)
		{
			Vlog::Module* result = new Vlog::Module();
			result->set_name(name);

			result->add_param(new Vlog::Parameter("NI", result));
			result->add_param(new Vlog::Parameter("WIDTH", result));

			result->add_port(new Vlog::Port("clk", result, 1, Vlog::Port::IN));
			result->add_port(new Vlog::Port("reset", result, 1, Vlog::Port::IN));
			result->add_port(new Vlog::Port("i_data", result, "NI*WIDTH", Vlog::Port::IN));
			result->add_port(new Vlog::Port("i_valid", result, "NI", Vlog::Port::IN));
			result->add_port(new Vlog::Port("i_eop", result, "NI", Vlog::Port::IN));
			result->add_port(new Vlog::Port("o_ready", result, "NI", Vlog::Port::OUT));
			result->add_port(new Vlog::Port("o_valid", result, 1, Vlog::Port::OUT));
			result->add_port(new Vlog::Port("o_eop", result, 1, Vlog::Port::OUT));
			result->add_port(new Vlog::Port("o_data", result, "WIDTH", Vlog::Port::OUT));
			result->add_port(new Vlog::Port("i_ready", result, 1, Vlog::Port::IN));

			return result;
		}

		void parameterize(P2P::Node* generic_node, Vlog::Instance* vinst)
		{
			using namespace P2P;

			MergeNode* node = (MergeNode*)generic_node;
			assert(node->get_type() == Node::MERGE);

			int data_width = node->get_proto().get_phys_field("xdata")->width;
			data_width = std::max(1, data_width);
			
			vinst->set_param_value("NI", node->get_n_inputs());
			vinst->set_param_value("WIDTH", data_width);
		}

		void get_port_name(P2P::Port* port, P2P::PhysField* pfield, Vlog::Instance* inst, GPNInfo* result)
		{
			using namespace P2P;

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

			int base_idx = 0;
			if (port->get_name() == "out")
			{
				if (pfield->name == "valid") result->port = "o_valid";
				else if (pfield->name == "ready") result->port = "i_ready";
				else if (pfield->name == "eop") result->port = "o_eop";
				else result->port = "o_data";
				
				result->lo = 0;
			}
			else
			{
				auto parent = (P2P::MergeNode*) port->get_parent();
				int port_idx = parent->get_inport_idx(port);

				if (pfield->name == "valid") result->port = "i_valid";
				else if (pfield->name == "ready") result->port = "o_ready";
				else if (pfield->name == "eop") result->port = "i_eop";
				else result->port = "i_data";

				result->lo = port_idx * pfield->width;
			}
		}
	} s_impl;

	ImplVerilog::BuiltinReg s_impl_reg ([]
	{
		ImplVerilog::register_impl(P2P::Node::MERGE, &s_impl);
	});
}