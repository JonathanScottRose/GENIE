#include <fstream>
#include "genie/vlog.h"
#include "genie/structure.h"

using namespace genie::vlog;

// Humble Verilog Writer

namespace
{
	std::ofstream s_file;
	int s_cur_indent;
	const int INDENT_AMT = 4;

	std::string format_port_bindings(Port*);

	void write_line(const std::string& line, bool indent = true, bool newline = true);
	void write_port(Port* port);
	void write_param(genie::ParamBinding* param);
	void write_net(Net* net);
	void write_inst_portbindings(Port* binding);
	void write_inst_parambinding(genie::ParamBinding* binding);

	void write_sys_ports(SystemVlogInfo* mod);
	void write_sys_params(SystemVlogInfo* mod);
	void write_sys_wires(SystemVlogInfo* mod);
	void write_sys_insts(SystemVlogInfo* mod);
	void write_sys_file(SystemVlogInfo* mod);
	void write_sys_body(SystemVlogInfo* mod);
	void write_sys_localparams(SystemVlogInfo* mod);
	void write_sys_assigns(SystemVlogInfo* mod);

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
		std::string dir_str = 
			port->get_dir() == Port::OUT ? "output " : 
			port->get_dir() == Port::IN ? "input " : "inout";

		std::string size_str;

		int width = port->eval_width();
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

	void write_wire(Net* net)
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

	void write_sys_assigns(SystemVlogInfo* mod)
	{
		// All bindings to 'output/inout' top-level Ports are done through 'assign' statements.

		for (auto& i : mod->ports())
		{
			Port* port = i.second;
			if (port->get_dir() == Port::IN)
				continue;

			// Don't emit an assign statement when there's nothing to assign
			if (!port->is_bound())
				continue;

			// Reuse the same formatting code that prints port bindings on instances
			std::string line = "assign " + port->get_name() + " = " + format_port_bindings(port) + ";";
			write_line(line);
		}
	}

	void write_sys_ports(SystemVlogInfo* mod)
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

	void write_sys_params(SystemVlogInfo* mod)
	{
		auto params = mod->get_node()->get_params(false);
		
		if (!params.empty())
		{
			write_line(" #", false, true);
			write_line("(");
			s_cur_indent++;

			int parmno = params.size();
			for (auto i : params)
			{
				write_param(i);
				if (--parmno > 0) write_line(",", false, true);
			}
			write_line("", false, true);

			s_cur_indent--;
			write_line(")", true, false);
		}

		write_line("", false, true);
	}


	void write_sys_wires(SystemVlogInfo* mod)
	{
		for (auto& i : mod->nets())
		{
			Net* net = i.second;

			// Don't write EXPORT nets, only wires
			if (net->get_type() != Net::WIRE)
				continue;

			write_wire(net);
		}
	}

	std::string format_port_bindings(Port* ps)
	{
		std::string bindstr = "";

		// Can we tie constants to this port?
		bool can_tie = ps->get_dir() != Port::OUT;

		// Keep track of whether or not we wrote multiple comma-separated items
		bool had_commas = false;

		if (ps->is_bound())
		{
			// Sort bindings in most to least significant bit order
			auto sorted_bindings = ps->bindings();
			std::sort(sorted_bindings.begin(), sorted_bindings.end(), [](PortBinding* l, PortBinding* r)
			{
				return l->get_port_lsb() > r->get_port_lsb();
			});

			// Now traverse the port's bits from MSB to LSB and connect either bindings (const or net),
			// or unconnected bits (hi-impedance)
			auto binding_iter = sorted_bindings.begin();
			const auto& binding_iter_end = sorted_bindings.end();
			int cur_bit = ps->eval_width();

			while (cur_bit > 0)
			{
				// Grab the next binding, in descending connected bit order
				PortBinding* binding = (binding_iter == binding_iter_end)? nullptr : *binding_iter;

				// Find out where the next connected-to-something bit is. If there's no next binding,
				// then this is the port's LSB and it's all Z's from here on
				int next_binding_hi = binding? (binding->get_port_lsb() + binding->get_width()) : 0;

				// The number of unconnected bits from the current bit forwards
				int unconnected = cur_bit - next_binding_hi;
				if (unconnected)
				{
					// Put hi-impedance for the unconnected bits.
					// Only inputs may be left unconnected: can't connect an output port to constant
					// z's
					assert(can_tie);

					// Try and make it pretty. There's two ways we can write a run of z's:
					// 1) 5'bzzzzz
					// 2) {5'{1'bz}}
					// So let's try both and print out the one that's shorter.
					std::string version1 =
						std::to_string(unconnected) + "'b" + std::string(unconnected, 'z');
					std::string version2 =
						"{" + std::to_string(unconnected) + "{1'bz}}";

					bindstr += version2.length() < version1.length()? version2 : version1;

					// Advance to the next connected thing (or the LSB if there's nothing left)
					cur_bit = next_binding_hi;
				}
				else
				{
					// Otherwise, cur_bit points to the top of the next binding. Write it out.			
					Bindable* targ = binding->get_target();
					bindstr += targ->to_string();
					
					// Do a range select on the target if not binding to the full target
					if (!binding->is_full_target_binding())
					{
						int bind_width = binding->get_width();
						int bind_lsb = binding->get_target_lsb();

						bindstr += "[";
						if (bind_width > 1)
						{
							// If the part-select is greater than 1 bit, we need this
							bindstr += std::to_string(bind_lsb + bind_width - 1);
							bindstr += ":";
						}
						bindstr += std::to_string(bind_lsb);
						bindstr += "]";
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
		std::string opener = had_commas ? "{" : "";
		std::string closer = had_commas ? "}" : "";

		return opener + bindstr + closer;
	}

	void write_inst_portbindings(Port* ps)
	{
		const std::string& portname = ps->get_name();
		std::string bindings = format_port_bindings(ps);
		write_line("." + portname + "(" + bindings + ")", true, false);
	}

	void write_inst_parambinding(genie::ParamBinding* binding)
	{
		const std::string& paramname = binding->get_name();
		std::string paramval = binding->get_expr().to_string();
		write_line("." + paramname + "(" + paramval + ")", true, false);
	}

	void write_sys_insts(SystemVlogInfo* mod)
	{
		auto sys = static_cast<genie::System*>(mod->get_node());

		write_line("");
		
		// Every genie::Node inside a genie::System gets an instance named after it.
		// Is this too coupled?
		auto nodes = sys->get_nodes();
		for (auto& node : nodes)
		{
			auto vinfo = static_cast<NodeVlogInfo*>(node->get_hdl_info());

			// Gather bound params only (ones with a value attached to them)
			auto bound_params = node->get_params(true);

			// Write out parameter bindings for this instance
			bool has_params = !bound_params.empty();

			write_line(vinfo->get_module_name() + " ", true, false);
			if (has_params)
			{
				write_line("#", false, true);
				write_line("(");
				s_cur_indent++;

				int paramno = bound_params.size();
				for (auto b : bound_params)
				{
					write_inst_parambinding(b);

					if (--paramno != 0)
						write_line(",", false, true);
				}

				write_line("", false, true);
				s_cur_indent--;
				write_line(")");
				write_line("", true, false);
			}

			// The instance name (same name as the Node)
			write_line(node->get_name(), false, true);
			write_line("(");
			s_cur_indent++;

			// Write port bindings
			int portno = vinfo->ports().size();
			for (auto& j :vinfo->ports())
			{
				Port* ps = j.second;
				write_inst_portbindings(ps);

				if (--portno != 0)
					write_line(",", false, true);
			}

			write_line("", false, true);
			s_cur_indent--;
			write_line(");");
			write_line("");
		}
	}

	void write_sys_localparams(SystemVlogInfo* mod)
	{
		// Write Params of the System that have concrete values bound already.
		genie::Node* sysnode = mod->get_node();
		auto bound_params = sysnode->get_params(true);

		for (auto i : bound_params)
		{
			write_line("localparam " + i->get_name() + " = " + i->get_expr().to_string() + ";");
		}

		write_line("");
	}

	void write_sys_body(SystemVlogInfo* mod)
	{
		s_cur_indent++;

		write_sys_localparams(mod);
		write_sys_wires(mod);
		write_sys_insts(mod);
		write_sys_assigns(mod);

		s_cur_indent--;
	}

	void write_sys_file(SystemVlogInfo* mod)
	{
		const std::string& mod_name = mod->get_module_name();

		write_line("module " + mod_name, true, false);

		write_sys_params(mod);

		write_line("(");
		write_sys_ports(mod);
		write_line(");");

		write_sys_body(mod);

		write_line("endmodule");
	}
}

void genie::vlog::write_system(genie::System* sys)
{
	auto top = as_a<SystemVlogInfo*>(sys->get_hdl_info());
	std::string filename = top->get_module_name() + ".sv";

	s_file.open(filename);
	write_sys_file(top);
	s_file.close();
}
