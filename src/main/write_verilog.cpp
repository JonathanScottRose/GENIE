#include <fstream>
#include "build_spec.h"
#include "write_verilog.h"
#include "ct/p2p.h"
#include "ct/spec.h"
#include "ct/ct.h"
#include "ct/instance_node.h"

using namespace ct;
using namespace ct::Spec;
using namespace ct::P2P;

namespace
{
	std::ofstream s_file;
	SystemNode* s_root_node;
	int s_cur_indent;
	const int INDENT_AMT = 4;

	void write_line(const std::string& line, bool indent = true, bool newline = true)
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

	std::string make_sig_name(const std::string& basename, const std::string& signame)
	{
		return basename + '_' + signame;
	}

	bool is_top_level_port(Port* port)
	{
		return (port->get_parent() == s_root_node);
	}

	void write_sig(bool output, int hi, int lo, const std::string& signame, bool first)
	{
		std::string dir_str = output? "output " : "input ";
		std::string size_str = (hi == lo) ? "" : 
			"[" + std::to_string(hi) + ":" + std::to_string(lo) + "] ";

		if (!first)
		{
			write_line(",", false, true);
		}

		write_line(dir_str + size_str + signame, true, false);
	}

	void write_wire(int hi, int lo, const std::string& signame)
	{
		std::string size_str = (hi == lo) ? "" : 
			"[" + std::to_string(hi) + ":" + std::to_string(lo) + "] ";
			
		write_line("wire " + size_str + signame + ";");
	}

	void write_sys_ports()
	{
		s_cur_indent++;
		bool first_sig = true;

		for (auto& i : s_root_node->ports())
		{
			Port* port = i.second;
			auto& base_name = i.first;

			switch (port->get_type())
			{
			case Port::RESET:
			case Port::CLOCK:
				write_sig(Port::IN, 0, 0, base_name, first_sig);
				first_sig = false;
				break;

			case Port::DATA:
				{
					DataPort* dport = (DataPort*)port;
					bool port_o = dport->get_dir() == Port::IN; // reversed sense
					const Protocol& proto = dport->get_proto();

					for (Protocol::Field* field : proto.fields())
					{
						bool sig_o = port_o ^ (field->sense == Protocol::Field::FWD);

						write_sig(
							sig_o,
							field->width - 1,
							0,
							make_sig_name(base_name, field->name),
							first_sig
						);

						first_sig = false;
					}
				}
				break;

			default:
				assert(false);
				break;
			} // port type
		} // next node port

		write_line("", false, true);

		s_cur_indent--;
	}

	void write_sys_nets()
	{
		write_line("// Nets");
		
		bool first_conn = true;

		for (Conn* conn : s_root_node->conns())
		{
			Port* src = conn->get_source();
			Port* sink = conn->get_sink();
			Node* src_node = src->get_parent();
						
			// Only create wires for internal connections
			if (is_top_level_port(src) || is_top_level_port(sink))
				continue;

			if (!first_conn)
				write_line("");
			first_conn = false;

			std::string basename = src_node->get_name() + "_" + src->get_name();

			switch (src->get_type())
			{
			case Port::RESET:
			case Port::CLOCK:
				write_wire(0, 0, basename);
				break;

			case Port::DATA:
				{
					const Protocol& proto = ((DataPort*)src)->get_proto();

					for (Protocol::Field* f : proto.fields())
					{
						write_wire(
							f->width - 1,
							0,
							make_sig_name(basename, f->name)
						);
					}
				}
				break;

			default:
				assert(false);
				break;
			}
		}
	}

	void write_port_conn(const std::string& portname, const std::string& connname, bool first)
	{
		if (!first)
			write_line(",", false, true);

		write_line("." + portname + "(" + connname + ")", true, false);			
	}

	std::string get_driver(Port* port)
	{
		Port* driver = port->get_conn()->get_source();
		Node* driver_node = driver->get_parent();

		if (is_top_level_port(driver))
		{
			return driver->get_name();
		}
		else
		{
			return driver_node->get_name() + "_" + driver->get_name();
		}
	}

	void write_node_insts()
	{
		using namespace BuildSpec;

		write_line("// Instances");
		bool first_inst = true;

		for (auto& it : s_root_node->nodes())
		{
			Node* node = it.second;

			if (node->get_type() != Node::INSTANCE)
				assert(false); // for now

			InstanceNode* inode = (InstanceNode*)node;
			Instance* inst_def = inode->get_instance();
			Component* comp_def = Spec::get_component_for_instance(inst_def->get_name());
			ComponentImpl* comp_impl = (ComponentImpl*)comp_def->get_impl();

			if (!first_inst)
				write_line("");
			first_inst = false;

			write_line(comp_impl->module_name + " " + inode->get_name());
			write_line("(");
			s_cur_indent++;

			bool first_line = true;

			for (auto& j : inode->ports())
			{
				Port* port = j.second;
				std::string drv_basename = get_driver(port);			

				switch(port->get_type())
				{
				case Port::CLOCK:
				case Port::RESET:
					{
						ClockResetInterface* iface = (ClockResetInterface*)port->get_iface_def();
						Signal* sig_def = iface->signals().front();
						BuildSpec::SignalImpl* sig_impl = (BuildSpec::SignalImpl*)sig_def->get_impl();
						write_port_conn(sig_impl->signal_name, drv_basename, first_line);
						first_line = false;
					}
					break;
				case Port::DATA:
					{
						DataPort* dport = (DataPort*)port;
						const Protocol& proto = dport->get_proto();

						for (Protocol::Field* field : proto.fields())
						{
							Signal* sig_def = field->sigdef;
							BuildSpec::SignalImpl* sig_impl = 
								(BuildSpec::SignalImpl*)sig_def->get_impl();
							
							write_port_conn(
								sig_impl->signal_name,
								make_sig_name(drv_basename, field->name),
								first_line
							);

							first_line = false;
						}
					}
					break;
				default:
					assert(false);
				}
			}

			s_cur_indent--;
			write_line("", false, true);
			write_line(");");
		}
	}

	void write_sys_body()
	{
		s_cur_indent++;

		write_sys_nets();
		write_node_insts();

		s_cur_indent--;
	}

	void write_sys_file()
	{
		const std::string& mod_name = s_root_node->get_name();

		write_line("module " + mod_name);
		write_line("(");
		write_sys_ports();
		write_line(");");

		write_sys_body();

		write_line("endmodule");
	}
}

void WriteVerilog::go()
{
	s_root_node = ct::get_root_node();
	std::string filename = s_root_node->get_name() + ".v";

	s_file.open(filename);
	write_sys_file();
	s_file.close();
}