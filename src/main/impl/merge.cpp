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

		Vlog::Module* implement(ct::P2P::Node* generic_node)
		{
			Vlog::Module* result = new Vlog::Module();
			result->set_name(get_module_name(generic_node));

			result->add_param(new Vlog::Parameter("NI", result));
			result->add_param(new Vlog::Parameter("WIDTH", result));
			result->add_param(new Vlog::Parameter("EOP_LOC", result));

			result->add_port(new Vlog::Port("clk", result, 1, Vlog::Port::IN));
			result->add_port(new Vlog::Port("reset", result, 1, Vlog::Port::IN));
			result->add_port(new Vlog::Port("i_data", result, "NI*WIDTH", Vlog::Port::IN));
			result->add_port(new Vlog::Port("i_valid", result, "NI", Vlog::Port::IN));
			result->add_port(new Vlog::Port("o_ready", result, "NI", Vlog::Port::OUT));
			result->add_port(new Vlog::Port("o_valid", result, 1, Vlog::Port::OUT));
			result->add_port(new Vlog::Port("o_data", result, "WIDTH", Vlog::Port::OUT));
			result->add_port(new Vlog::Port("i_ready", result, 1, Vlog::Port::IN));

			return result;
		}

		void parameterize(P2P::Node* generic_node, Vlog::Instance* vinst)
		{
			using namespace P2P;

			MergeNode* node = (MergeNode*)generic_node;
			assert(node->get_type() == Node::MERGE);

			int data_width = 0;
			for (auto& i : node->get_proto().fields())
			{
				if (i->name == "eop")
				{
					vinst->set_param_value("EOP_LOC", data_width);
				}
				
				if (i->name != "valid" && i->name != "ready")
				{
					data_width += i->width;
				}
			}
			
			vinst->set_param_value("NI", node->get_n_inputs());
			vinst->set_param_value("WIDTH", data_width);
		}

		void get_port_name(P2P::Port* port, P2P::Field* field, Vlog::Instance* inst,
			ImplVerilog::GPNInfo* result)
		{
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
			
			int base_idx = 0;
			if (port->get_name() == "out")
			{
				if (field->name == "valid")
				{
					result->port = "o_valid";
					result->lo = 0;
					return;
				}
				else if (field->name == "ready")
				{
					result->port = "i_ready";
					result->lo = 0;
					return;
				}
				else
				{
					result->port = "o_data";
				}
			}
			else
			{
				int port_idx = std::stoi(port->get_name().substr(2));
				if (field->name == "valid")
				{
					result->port = "i_valid";
					result->lo = port_idx;
					return;
				}
				else if (field->name == "ready")
				{
					result->port = "o_ready";
					result->lo = port_idx;
					return;
				}
				else
				{
					result->port = "i_data";
					base_idx = port_idx * inst->get_param_value("WIDTH");
				}
			}

			for (P2P::Field* f : port->get_proto().fields())
			{
				if (f->name == field->name)
					break;
				else if (f->name != "valid" && f->name != "ready")
					base_idx += f->width;
			}

			result->lo = base_idx;
		}
	} s_impl;

	ImplVerilog::BuiltinReg s_impl_reg ([]
	{
		ImplVerilog::register_impl(P2P::Node::MERGE, &s_impl);
	});
}