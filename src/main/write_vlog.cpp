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
	void write_port_binding(PortBinding* binding);

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
		if (port->get_is_vec())
		{
			int hi = port->get_width() - 1;
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

			portno--;
			if (portno > 0) write_line(",", false, true);
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

	void write_port_binding(PortBinding* binding)
	{
		const std::string& portname = binding->get_name();
		const std::string& netname = binding->get_net()->get_name();
		
		write_line("." + portname + "(" + netname + ")", true, false);	
	}

	void write_sys_insts(SystemModule* mod)
	{
		write_line("");
		
		for (auto& i : mod->instances())
		{
			Instance* inst = i.second;
			Module* mod = inst->get_module();

			write_line(mod->get_name() + " " + inst->get_name());
			write_line("(");
			s_cur_indent++;

			int portno = inst->port_bindings().size();
			for (auto& j : inst->port_bindings())
			{
				PortBinding* binding = j.second;
				write_port_binding(binding);

				portno--;
				if (portno != 0)
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