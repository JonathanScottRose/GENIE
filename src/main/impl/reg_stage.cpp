#include "impl_vlog.h"
#include "ct/reg_node.h"

using namespace ct;

namespace
{
	class : public ImplVerilog::IModuleImpl
	{
		const std::string& get_module_name(ct::P2P::Node* node)
		{
			static std::string nm = "ct_pipe_stage";
			return nm;
		}

		Vlog::Module* implement(ct::P2P::Node* generic_node, const std::string& name)
		{
			Vlog::Module* result = new Vlog::Module();
			result->set_name(name);

			result->add_param(new Vlog::Parameter("WIDTH", result));

			result->add_port(new Vlog::Port("i_clk", result, 1, Vlog::Port::IN));
			result->add_port(new Vlog::Port("i_reset", result, 1, Vlog::Port::IN));
			result->add_port(new Vlog::Port("i_data", result, "WIDTH", Vlog::Port::IN));
			result->add_port(new Vlog::Port("i_valid", result, 1, Vlog::Port::IN));
			result->add_port(new Vlog::Port("o_ready", result, 1, Vlog::Port::OUT));
			result->add_port(new Vlog::Port("o_valid", result, 1, Vlog::Port::OUT));
			result->add_port(new Vlog::Port("o_data", result, "WIDTH", Vlog::Port::OUT));
			result->add_port(new Vlog::Port("i_ready", result, 1, Vlog::Port::IN));

			return result;
		}

		
		void parameterize(P2P::Node* generic_node, Vlog::Instance* vinst)
		{
			using namespace P2P;

			RegNode* node = (RegNode*)generic_node;
			assert(node->get_type() == Node::REG_STAGE);

			int data_width = node->get_inport()->get_proto().get_phys_field("xdata")->width;

			vinst->set_param_value("WIDTH", data_width);
		}

		void get_port_name(P2P::Port* port, P2P::PhysField* pfield, Vlog::Instance* inst, GPNInfo* result)
		{
			using namespace P2P;

			RegNode* node = (RegNode*)port->get_parent();

			if (port->get_name() == "clock")
			{
				result->port = "i_clk";
				result->lo = 0;
				return;
			}
			else if (port->get_name() == "reset")
			{
				result->port = "i_reset";
				result->lo = 0;
				return;
			}

			const Protocol& proto = port->get_proto();

			if (port->get_name() == "in")
			{
				if (pfield->name == "valid") result->port = "i_valid";
				else if (pfield->name == "ready") result->port = "o_ready";
				else if (pfield->name == "xdata") result->port = "i_data";
				else assert(false);

				result->lo = 0;
			}
			else if (port->get_name() == "out")
			{
				if (pfield->name == "valid") result->port = "o_valid";
				else if (pfield->name == "ready") result->port = "i_ready";
				else if (pfield->name == "xdata") result->port = "o_data";
				else assert(false);

				result->lo = 0;
			}
		}
	} s_impl;

	ImplVerilog::BuiltinReg s_impl_reg([]
	{
		ImplVerilog::register_impl(P2P::Node::REG_STAGE, &s_impl);
	});
}
