#include "impl_vlog.h"
#include "ct/clock_cross_node.h"

using namespace ct;

namespace
{
	class : public ImplVerilog::IModuleImpl
	{
		const std::string& get_module_name(ct::P2P::Node* node)
		{
			static std::string nm = "ct_clock_cross";
			return nm;
		}

		Vlog::Module* implement(ct::P2P::Node* generic_node, const std::string& name)
		{
			Vlog::Module* result = new Vlog::Module();
			result->set_name(name);

			result->add_param(new Vlog::Parameter("WIDTH", result));

			result->add_port(new Vlog::Port("arst", result, 1, Vlog::Port::IN));

			result->add_port(new Vlog::Port("wrclk", result, 1, Vlog::Port::IN));
			result->add_port(new Vlog::Port("i_data", result, "WIDTH", Vlog::Port::IN));
			result->add_port(new Vlog::Port("i_valid", result, 1, Vlog::Port::IN));
			result->add_port(new Vlog::Port("o_ready", result, 1, Vlog::Port::OUT));

			result->add_port(new Vlog::Port("rdclk", result, 1, Vlog::Port::IN));
			result->add_port(new Vlog::Port("o_data", result, "WIDTH", Vlog::Port::OUT));
			result->add_port(new Vlog::Port("o_valid", result, 1, Vlog::Port::OUT));
			result->add_port(new Vlog::Port("i_ready", result, 1, Vlog::Port::IN));

			return result;
		}

		void parameterize(P2P::Node* generic_node, Vlog::Instance* vinst)
		{
			using namespace P2P;

			ClockCrossNode* node = (ClockCrossNode*)generic_node;
			assert(node->get_type() == Node::CLOCK_CROSS);

			int data_width = node->get_inport()->get_proto().get_phys_field("xdata")->width;
			vinst->set_param_value("WIDTH", data_width);
		}

		void get_port_name(P2P::Port* port, P2P::PhysField* pfield, Vlog::Instance* inst, GPNInfo* result)
		{
			using namespace P2P;

			ClockCrossNode* node = (ClockCrossNode*)port->get_parent();

			if (port->get_name() == "reset")
			{
				result->port = "reset";
				result->lo = 0;
				return;
			}
			else if (port->get_name() == "in_clock")
			{
				result->port = "wrclk";
				result->lo = 0;
				return;
			}
			else if (port->get_name() == "out_clock")
			{
				result->port = "rdclk";
				result->lo = 0;
				return;
			}

			const Protocol& proto = port->get_proto();

			if (port->get_name() == "in_data")
			{
				if (pfield->name == "valid") result->port = "i_valid";
				else if (pfield->name == "ready") result->port = "o_ready";
				else result->port = "i_data";

				result->lo = 0;
			}
			else if (port->get_name() == "out_data")
			{
				if (pfield->name == "valid") result->port = "o_valid";
				else if (pfield->name == "ready") result->port = "i_ready";
				else result->port = "o_data";

				result->lo = 0;
			}
		}
	} s_impl;

	ImplVerilog::BuiltinReg s_impl_reg([]
	{
		ImplVerilog::register_impl(P2P::Node::CLOCK_CROSS, &s_impl);
	});
}
