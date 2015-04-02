#include <fstream>
#include "genie/vlog.h"
#include "genie/structure.h"

using namespace genie::vlog;

namespace
{
	std::ofstream s_file;
	int s_cur_indent;
	const int INDENT_AMT = 4;

	void write_line(const std::string& line, bool indent = true, bool newline = true);
	void write_port(Port* port);
	void write_param(genie::ParamBinding* param);
	void write_wire(WireNet* net);
	void write_port_bindings(PortBinding* binding);
	void write_param_binding(genie::ParamBinding* binding);

	void write_sys_ports(SystemModule* mod);
	void write_sys_params(SystemModule* mod);
	void write_sys_nets(SystemModule* mod);
	void write_sys_file(SystemModule* mod);
	void write_sys_body(SystemModule* mod);
	void write_sys_localparams(SystemModule* mod);

	void write_line(const std::string& line, bool indent, bool newline)
	{
		if (indent)
		{
			for (int i = 0; i < s_cur_indent * INDENT_AMT; i++)
				s_file << ' ';
		}

		s_file << line;

		if (newline)
			s_file << std::endl;
	}

	void write_port(Port* port)
	{
		std::string dir_str = port->get_dir() == Port::OUT ? "output " : "input ";
		std::string size_str;

		int width = port->get_width().get_value();
		if (width > 1)
		{
			int hi = width - 1;
			int lo = 0;
			size_str = "[" + std::to_string(hi) + ":" + std::to_string(lo) + "] ";
		}

		write_line(dir_str + size_str + port->get_name(), true, false);
	}

	void write_param(genie::ParamBinding* param)
	{
		write_line("parameter " + param->get_name(), true, false);
	}

	void write_wire(WireNet* net)
	{
		std::string size_str;
		if (net->get_width() == 1)
		{
			size_str = "";
		}
		else
		{
			int hi = net->get_width() - 1;
			int lo = 0;
			size_str = "[" + std::to_string(hi) + ":" + std::to_string(lo) + "] ";
		}
			
		write_line("wire " + size_str + net->get_name() + ";");
	}

	void write_sys_ports(SystemModule* mod)
	{
		s_cur_indent++;
		
		int portno = mod->ports().size();
		for (auto& i : mod->ports())
		{
			write_port(i.second);
			if (--portno > 0) write_line(",", false, true);
		}

		write_line("", false, true);

		s_cur_indent--;
	}

	void write_sys_params(SystemModule* mod)
	{
		using genie::Node;

		auto& all_params = mod->get_sysnode()->params();
		Node::Params params; 

		for (auto& i : mod->get_sysnode()->params())
		{
			if (!i.second->is_bound())
				params.insert(i);
		}
		
		if (!params.empty())
		{
			write_line(" #", false, true);
			write_line("(");
			s_cur_indent++;

			int parmno = params.size();
			for (auto& i : params)
			{
				write_param(i.second);
				if (--parmno > 0) write_line(",", false, true);
			}
			write_line("", false, true);

			s_cur_indent--;
			write_line(")", true, false);
		}

		write_line("", false, true);
	}


	void write_sys_nets(SystemModule* mod)
	{
		for (auto& i : mod->nets())
		{
			WireNet* net = (WireNet*)i.second;
			if (net->get_type() != Net::WIRE)
				continue;

			write_wire(net);
		}
	}

	void write_port_bindings(PortState* ps)
	{
		const std::string& portname = ps->get_name();
		std::string bindstr = "";

		bool is_input = ps->get_port()->get_dir() == Port::IN;

		// Keep track of whether or not we wrote multiple comma-separated items
		bool had_commas = false;

		if (!ps->is_empty())
		{
			// Sort bindings in most to least significant bit order
			auto sorted_bindings = ps->bindings();
			std::sort(sorted_bindings.begin(), sorted_bindings.end(), [](PortBinding* l, PortBinding* r)
			{
				return l->get_port_lo() > r->get_port_lo();
			});

			// Now traverse the port's bits from MSB to LSB and connect either bindings (const or net),
			// or unconnected bits (hi-impedance)
			auto binding_iter = sorted_bindings.begin();
			const auto& binding_iter_end = sorted_bindings.end();
			int cur_bit = ps->get_width();

			while (cur_bit > 0)
			{
				// Grab the next binding, in descending connected bit order
				PortBinding* binding = (binding_iter == binding_iter_end)? nullptr : *binding_iter;

				// Find out where the next connected-to-something bit is. If there's no next binding,
				// then this is the port's LSB and it's all Z's from here on
				int next_binding_hi = binding? (binding->get_port_lo() + binding->get_width()) : 0;

				// The number of unconnected bits from the current bit forwards
				int unconnected = cur_bit - next_binding_hi;
				if (unconnected)
				{
					// Put hi-impedance for the unconnected bits.
					// Only inputs may be left unconnected: can't connect an output port to constant
					// z's
					assert(is_input);
					bindstr += std::to_string(unconnected) + "'b" + std::string(unconnected, 'z');

					// Advance to the next connected thing (or the LSB if there's nothing left)
					cur_bit = next_binding_hi;
				}
				else
				{
					// Otherwise, cur_bit points to the top of the next binding. Write it out.			
					Bindable* targ = binding->get_target();
					if (targ->get_type() == Bindable::CONST)
					{
						auto cv = (ConstValue*)targ;
						bindstr += std::to_string(cv->get_width());
						bindstr += "'d";
						bindstr += std::to_string(cv->get_value());
					}
					else
					{
						assert(targ->get_type() == Bindable::NET);
						auto net = (Net*)targ;

						bindstr += net->get_name();
						if (!binding->is_full_target_binding())
						{
							bindstr += "[";
							bindstr += std::to_string(binding->get_target_lo() + binding->get_width() - 1);
							bindstr += ":";
							bindstr += std::to_string(binding->get_target_lo());
							bindstr += "]";
						}
					}

					// Advance to whatever's after this binding (could be another binding or
					// some unconnected bits)
					cur_bit -= binding->get_width();
					++binding_iter;
				}

				// After whatever we just wrote (unconnectedness, or a binding), if there's more to
				// follow, then add a comma
				if (cur_bit > 0)
				{
					bindstr += ",";
					had_commas = true;
				}
			} // while cur_bit > 0
		} // non-empty
		
		// If we're binding a concatenation of multiple items (thus, we had to write commas), then
		// we need to surround the concatenation with curly braces
		std::string opener = had_commas ? "({" : "(";
		std::string closer = had_commas ? "})" : ")";

		write_line("." + portname + opener + bindstr + closer, true, false);
	}

	void write_param_binding(genie::ParamBinding* binding)
	{
		const std::string& paramname = binding->get_name();
		std::string paramval = binding->get_expr().to_string();
		write_line("." + paramname + "(" + paramval + ")", true, false);
	}

	void write_sys_insts(SystemModule* mod)
	{
		write_line("");
		
		for (auto& i : mod->instances())
		{
			Instance* inst = i.second;
			Module* mod = inst->get_module();
			
			auto& all_params = inst->get_node()->params();
			genie::Node::Params bound_params;

			for (auto& i : all_params)
			{
				if (i.second->is_bound())
					bound_params.insert(i);
			}

			bool has_params = !bound_params.empty();

			write_line(mod->get_name() + " ", true, false);
			if (has_params)
			{
				write_line("#", false, true);
				write_line("(");
				s_cur_indent++;

				int paramno = bound_params.size();
				for (auto& j : bound_params)
				{
					genie::ParamBinding* b = j.second;
					write_param_binding(b);

					if (--paramno != 0)
						write_line(",", false, true);
				}

				write_line("", false, true);
				s_cur_indent--;
				write_line(")");
				write_line("", true, false);
			}

			write_line(inst->get_name(), false, true);
			write_line("(");
			s_cur_indent++;

			int portno = inst->port_states().size();
			for (auto& j : inst->port_states())
			{
				PortState* ps = j.second;
				write_port_bindings(ps);

				if (--portno != 0)
					write_line(",", false, true);
			}

			write_line("", false, true);
			s_cur_indent--;
			write_line(");");		
		}
	}

	void write_sys_localparams(SystemModule* mod)
	{
		genie::System* sysnode = mod->get_sysnode();

		auto& all_params = sysnode->params();
		genie::Node::Params bound_params;
		for (auto& i : all_params)
		{
			if (i.second->is_bound())
				bound_params.insert(i);
		}

		for (auto& i : bound_params)
		{
			write_line("localparam " + i.first + " = " + i.second->get_expr().to_string() + ";");
		}
	}

	void write_sys_body(SystemModule* mod)
	{
		s_cur_indent++;

		write_sys_localparams(mod);
		write_sys_nets(mod);
		write_sys_insts(mod);

		s_cur_indent--;
	}

	void write_sys_file(SystemModule* mod)
	{
		const std::string& mod_name = mod->get_name();


		write_line("module " + mod_name, true, false);

		write_sys_params(mod);

		write_line("(");
		write_sys_ports(mod);
		write_line(");");

		write_sys_body(mod);

		write_line("endmodule");
	}
}

void genie::vlog::write_system(SystemModule* top)
{
	std::string filename = top->get_name() + ".sv";

	s_file.open(filename);
	write_sys_file(top);
	s_file.close();
}
