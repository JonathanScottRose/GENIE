#include <fstream>
#include "write_vlog.h"
#include "impl_vlog.h"
#include "vlog.h"

using namespace Vlog;
using namespace ct;

namespace
{
	std::ofstream s_file;
	int s_cur_indent;
	const int INDENT_AMT = 4;

	void write_line(const std::string& line, bool indent = true, bool newline = true);
	void write_port(Port* port);
	void write_wire(WireNet* net);
	void write_port_bindings(PortBinding* binding);
	void write_param_binding(ParamBinding* binding);

	void write_sys_ports(SystemModule* mod);
	void write_sys_nets(SystemModule* mod);
	void write_sys_file(SystemModule* mod);
	void write_sys_body(SystemModule* mod);

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

		int width = port->get_width();
		if (width > 1)
		{
			int hi = width - 1;
			int lo = 0;
			size_str = "[" + std::to_string(hi) + ":" + std::to_string(lo) + "] ";
		}

		write_line(dir_str + size_str + port->get_name(), true, false);
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

		if (!ps->is_empty())
		{
			// Multiple bindings? Curly brackets
			if (ps->has_multiple_bindings())
			{
				bindstr = "{";
			}

			// Sort bindings in most to least significant bit order
			auto sorted_bindings = ps->bindings();
			std::sort(sorted_bindings.begin(), sorted_bindings.end(), [](PortBinding* l, PortBinding* r)
			{
				return l->get_port_lo() > r->get_port_lo();
			});

			int cur_bit = ps->get_width();
			for (PortBinding* binding : sorted_bindings)
			{
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

				// If this isn't the last binding, then add a comma before moving on to the next
				cur_bit -= binding->get_width();
				if (cur_bit > 0) bindstr += ",";

				// Make sure the order of bindings is descending and there are no unbound gaps in the port
				assert(cur_bit == binding->get_port_lo());
			}

			assert(cur_bit == 0);

			// Multiple bindings? End curly brackets
			if (ps->has_multiple_bindings())
			{
				bindstr += "}";
			}
		} // non-empty
		
		write_line("." + portname + "(" + bindstr + ")", true, false);	
	}

	void write_param_binding(ParamBinding* binding)
	{
		const std::string& paramname = binding->get_name();
		std::string paramval = binding->value().to_string();
		write_line("." + paramname + "(" + paramval + ")", true, false);
	}

	void write_sys_insts(SystemModule* mod)
	{
		write_line("");
		
		for (auto& i : mod->instances())
		{
			Instance* inst = i.second;
			Module* mod = inst->get_module();

			bool has_params = !inst->param_bindings().empty();

			write_line(mod->get_name() + " ", true, false);
			if (has_params)
			{
				write_line("#", false, true);
				write_line("(");
				s_cur_indent++;

				int paramno = inst->param_bindings().size();
				for (auto& j : inst->param_bindings())
				{
					ParamBinding* b = j.second;
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

	void write_sys_body(SystemModule* mod)
	{
		s_cur_indent++;

		write_sys_nets(mod);
		write_sys_insts(mod);

		s_cur_indent--;
	}

	void write_sys_file(SystemModule* mod)
	{
		const std::string& mod_name = mod->get_name();

		write_line("module " + mod_name);
		write_line("(");
		write_sys_ports(mod);
		write_line(");");

		write_sys_body(mod);

		write_line("endmodule");
	}
}

void WriteVerilog::go()
{
	SystemModule* top = (SystemModule*)ImplVerilog::get_top_module();
	std::string filename = top->get_name() + ".v";

	s_file.open(filename);
	write_sys_file(top);
	s_file.close();
}