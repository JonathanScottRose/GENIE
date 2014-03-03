#pragma once

#include "impl_vlog.h"
#include "ct/split_node.h"

using namespace ct;

namespace
{
	class : public ImplVerilog::IModuleImpl
	{
		const std::string& get_module_name(ct::P2P::Node* node)
		{
			static std::string nm = "ct_split";
			return nm;
		}

		Vlog::Module* implement(ct::P2P::Node* generic_node, const std::string& name)
		{
			Vlog::Module* result = new Vlog::Module();
			result->set_name(name);

			result->add_param(new Vlog::Parameter("NO", result));
			result->add_param(new Vlog::Parameter("WO", result));
			result->add_param(new Vlog::Parameter("NF", result));
			result->add_param(new Vlog::Parameter("WF", result));
			result->add_param(new Vlog::Parameter("FLOWS", result));
			result->add_param(new Vlog::Parameter("ENABLES", result));

			result->add_port(new Vlog::Port("clk", result, 1, Vlog::Port::IN));
			result->add_port(new Vlog::Port("reset", result, 1, Vlog::Port::IN));
			result->add_port(new Vlog::Port("i_data", result, "WO", Vlog::Port::IN));
			result->add_port(new Vlog::Port("i_flow", result, "WF", Vlog::Port::IN));
			result->add_port(new Vlog::Port("i_valid", result, 1, Vlog::Port::IN));
			result->add_port(new Vlog::Port("o_ready", result, 1, Vlog::Port::OUT));
			result->add_port(new Vlog::Port("o_valid", result, "NO", Vlog::Port::OUT));
			result->add_port(new Vlog::Port("o_data", result, "NO*WO", Vlog::Port::OUT));
			result->add_port(new Vlog::Port("o_flow", result, "NO*WF", Vlog::Port::OUT));
			result->add_port(new Vlog::Port("i_ready", result, "NO", Vlog::Port::IN));

			return result;
		}

		void parameterize(P2P::Node* generic_node, Vlog::Instance* vinst)
		{
			using namespace P2P;

			SplitNode* node = (SplitNode*)generic_node;
			assert(node->get_type() == Node::SPLIT);

			int data_width = node->get_proto().get_phys_field("xdata")->width;
			int fid_width = node->get_flow_id_width();

			vinst->set_param_value("NO", node->get_n_outputs());
			vinst->set_param_value("WO", data_width);
			vinst->set_param_value("NF", node->get_n_flows());
			vinst->set_param_value("WF", fid_width);

			// Until parameters can be expressed like this {1, 2, 3}, convert
			// these into a single big integers using bitwise arithmetic

			int flows_param = 0;
			int enables_param = 0;
			for (Flow* f : node->get_flows())
			{
				flows_param <<= fid_width;
				flows_param |= f->get_id();

				int enable_val = 0;
				for (auto& v : node->get_dests_for_flow(f->get_id()))
				{
					enable_val |= 1 << v;
				}

				enables_param <<= node->get_n_outputs();
				enables_param |= enable_val;
			}

			vinst->set_param_value("ENABLES", enables_param);
			vinst->set_param_value("FLOWS", flows_param);
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

			if (port->get_name() == "in")
			{
				if (pfield->name == "valid")	result->port = "i_valid";
				else if (pfield->name == "ready") result->port = "o_ready";
				else if (pfield->name == "flow_id") result->port = "i_flow";
				else result->port = "i_data";

				result->lo = 0;
			}
			else
			{
				auto parent = (P2P::SplitNode*)port->get_parent();
				int port_idx = parent->get_idx_for_outport(port);

				if (pfield->name == "valid") result->port = "o_valid";
				else if (pfield->name == "ready") result->port = "i_ready";
				else if (pfield->name == "flow_id") result->port = "o_flow";
				else result->port = "o_data";
				
				result->lo = port_idx * pfield->width;
			}
		}
	} s_impl;

	ImplVerilog::BuiltinReg s_impl_reg ([]
	{
		ImplVerilog::register_impl(P2P::Node::SPLIT, &s_impl);
	});
}