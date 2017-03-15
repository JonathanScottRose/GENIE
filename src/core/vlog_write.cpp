#include "pch.h"
#include "genie/log.h"
#include "node_system.h"
#include "hdl.h"
#include "vlog_write.h"
#include "params.h"

using namespace genie::impl::hdl;
using genie::impl::NodeSystem;
using genie::impl::Node;
using genie::impl::NodeParam;
using genie::impl::NodeStringParam;
using genie::impl::NodeIntParam;
using genie::impl::NodeBitsParam;
using genie::impl::IntExpr;
using genie::impl::BitsVal;

// Humble Verilog Writer

namespace
{
	std::ofstream s_file;
	int s_cur_indent;
	constexpr int INDENT_AMT = 4;

    std::string format_port_bindings_bits(const Port& port, int slice,
        std::vector<PortBinding>::iterator& bind_it, std::vector<PortBinding>::iterator bind_it_end);
	std::string format_port_bindings(const Port&);
    std::string format_size(int depth, int width);
    std::string format_bits_val(const BitsVal& val);
    std::string format_unconn(int bits);
    std::string format_bindable(Bindable* b);
    std::string format_part_select(int lo, int size);
    std::string format_param(NodeParam* param);

	void write_line(const std::string& line, bool indent = true, bool newline = true);

	void write_sys_port(const Port& port);
	void write_sys_param(const std::string& name);
    void write_sys_localparam(const std::string& name, NodeParam* param);
	void write_sys_net(const Net& net);
	void write_inst_portbindings(const Port& port);
	void write_inst_parambinding(const std::string& name, NodeParam* param);

	void write_sys_ports(HDLState& mod);
	void write_sys_params(HDLState& mod);
	void write_sys_nets(HDLState& mod);
	void write_sys_insts(HDLState& mod);
	void write_sys_file(HDLState& mod);
	void write_sys_body(HDLState& mod);
	void write_sys_localparams(HDLState& mod);
	void write_sys_assigns(HDLState& mod);

	void write_line(const std::string& line, bool indent, bool newline)
	{
		if (indent)
		{
			for (int i = 0; i < s_cur_indent * INDENT_AMT; i++)
				s_file << ' ';
		}

		s_file << line;

		if (newline)
			s_file << '\n';
	}

	void write_sys_port(const Port& port)
	{
		std::string dir_str = 
			port.get_dir() == Port::Dir::OUT ? "output " : 
			port.get_dir() == Port::Dir::IN ? "input " : "inout";

		std::string size_str = format_size(port.get_depth(), port.get_width());
        write_line(dir_str + size_str + port.get_name(), true, false);
	}

    void write_sys_param(const std::string& name)
    {
        write_line("parameter " + name, true, false);
    }

	void write_sys_localparam(const std::string& name, NodeParam* param)
	{
		std::string line = "localparam";

        // TODO: can we get rid of explicit sizes for BitsParams?
        if (param->get_type() == NodeParam::BITS)
        {
            auto bp = dynamic_cast<NodeBitsParam*>(param);
            auto& val = bp->get_val();
            line += " " + format_size(val.get_size(1), val.get_size(1));
        }

        line += " " + name + " = " + format_param(param) + ";";

		write_line(line, true, true);
	}

	void write_sys_net(const Net& net)
	{
		std::string size_str = format_size(net.get_depth(), net.get_width());
			
		write_line("wire " + size_str + net.get_name() + ";");
	}

	void write_sys_assigns(HDLState& mod)
	{
		// All bindings to 'output/inout' top-level Ports are done through 'assign' statements.

		for (const auto& i : mod.get_ports())
		{
			const Port& port = i.second;
			if (port.get_dir() == Port::Dir::IN)
				continue;

			// Don't emit an assign statement when there's nothing to assign
			if (!port.is_bound())
				continue;

			// Reuse the same formatting code that prints port bindings on instances
			std::string line = "assign " + port.get_name() + " = " + format_port_bindings(port) + ";";
			write_line(line);
		}
	}

	void write_sys_ports(HDLState& mod)
	{
		s_cur_indent++;
		
        const auto& ports = mod.get_ports();
        for (auto it = ports.begin(); it != ports.end(); ++it)
        {
            if (it != ports.begin())
                write_line(",", false, true);

            write_sys_port(it->second);
        }

		write_line("", false, true);

		s_cur_indent--;
	}

	void write_sys_params(HDLState& mod)
	{
		const auto& params = mod.get_node()->get_params();
		
        // Get names of unbound params
        std::vector<std::string> unbound_params;
        for (auto& it : params)
        {
            if (it.second->get_type() == NodeParam::SYS)
                unbound_params.push_back(it.first);
        }

		if (!unbound_params.empty())
		{
			write_line(" #", false, true);
			write_line("(");
			s_cur_indent++;

			for (auto& it = unbound_params.begin(); it != unbound_params.end(); ++it)
			{
                if (it != unbound_params.begin())
                    write_line(",", false, true);
				write_sys_param(*it);
			}
			write_line("", false, true);

			s_cur_indent--;
			write_line(")", true, false);
		}

		write_line("", false, true);
	}


	void write_sys_nets(HDLState& mod)
	{
		for (auto& i : mod.get_nets())
		{
			const Net& net = i.second;

			// Don't write EXPORT nets, only wires
			if (net.get_type() != Net::WIRE)
				continue;

			write_sys_net(net);
		}
	}

    std::string format_param(NodeParam* param)
    {
        std::string result;

        // Not done with virtual func polymorphism. Keep verilog-specific formatting separate
        // from internal representation. TODO: fancy fix?
        switch (param->get_type())
        {
        case NodeParam::STRING:
        {
            auto str_param = dynamic_cast<NodeStringParam*>(param);
            result = "\"" + str_param->get_val() + "\"";
            break;
        }
        case NodeParam::INT:
        {
            auto int_param = dynamic_cast<NodeIntParam*>(param);
            result = int_param->get_val().to_string();
            break;
        }
        case NodeParam::BITS:
        {
            auto bits_param = dynamic_cast<NodeBitsParam*>(param);
            auto& val = bits_param->get_val();
            result += format_bits_val(val);
            break;
        }
        default:
            assert(false);
        }

        return result;
    }

    std::string format_part_select(int lo, int size)
    {
        std::string result = "[";
        if (size > 1)
        {
            // If the part-select is greater than 1 bit, we need this
            result += std::to_string(lo + size - 1);
            result += ":";
        }
        result += std::to_string(lo);
        result += "]";

        return result;
    }

    std::string format_port_bindings_bits(const Port& port, int slice,
        std::vector<PortBinding>::iterator& bind_it, std::vector<PortBinding>::iterator bind_it_end)
    {
        std::vector<std::string> formatted_ranges;

        // Format bound bit-ranges within a slice. bind_it is an iterator to the next binding.
        // start from the MSB of the slice and work down.

        int cur_bit = port.get_width();
        while (cur_bit > 0)
        {
            // Grab the next binding, in descending connected bit order
            PortBinding* binding = (bind_it == bind_it_end)? nullptr : &(*bind_it);

            // If the next binding is in a different slice, consider it the same as there
            // not being a next binding (because there isn't any more... in this slice)
            if (binding && binding->get_port_lo_slice() != slice)
                binding = nullptr;

            // Find out where the next connected-to-something bit is. If there's no next binding,
            // then this is the port's LSB and it's all Z's from here on
            int next_binding_hi = binding? 
                (binding->get_port_lo_bit() + binding->get_bound_bits()) : 0;

            // The number of unconnected bits from the current bit forwards
            int unconnected = cur_bit - next_binding_hi;
            if (unconnected)
            {
                // Put hi-impedance for the unconnected bits.
                formatted_ranges.push_back(format_unconn(unconnected));

                // Advance to the next connected thing (or the LSB if there's nothing left)
                cur_bit = next_binding_hi;
            }
            else
            {
                // Otherwise, cur_bit points to the top of the next binding. Write it out.			
                Bindable* targ = binding->get_target();
                std::string bindstr = format_bindable(targ);

                // Do a range select on the target if not binding to the full target bits
                if (!binding->is_full0_target_binding())
                {
                    int bind_width = binding->get_bound_bits();
                    int bind_lsb = binding->get_target_lo_bit();

                    bindstr += format_part_select(bind_lsb, bind_width);
                }

                // Advance to whatever's after this binding (could be another binding or
                // some unconnected bits)
                cur_bit -= binding->get_bound_bits();
                ++bind_it;
            }
        } // while cur_bit > 0

        // Collapse the vector into a string
        std::string result;
        for (auto& it = formatted_ranges.begin(); it != formatted_ranges.end(); ++it)
        {
            // Add commas before every-except-first element
            if (it != formatted_ranges.begin())
                result += ",";
            result += *it;
        }

        // If there was more than one element, we need curly braces
        if (formatted_ranges.size() > 1)
            result = "{" + result + "}";

        return result;
    }


	std::string format_port_bindings(const Port& port)
	{
		std::vector<std::string> formatted_slices;

		// Can we tie constants to this port?
		bool can_tie = port.get_dir() != Port::Dir::OUT;

        // If the port is not bound at all, skip any formatting. Otherwise, we'd end up with a
        // technically correct, but ugly, litany of zzzzzzzz's
		if (port.is_bound())
		{
			// Sort bindings in most to least significant order: slices, then bits
			auto sorted_bindings = port.bindings();
			std::sort(sorted_bindings.begin(), sorted_bindings.end(), 
                [](const PortBinding& l, const PortBinding& r)
			{
				return l.get_port_lo_slice() > r.get_port_lo_slice() ||
                    l.get_port_lo_bit() > r.get_port_lo_bit();
			});

			// Now traverse the port's slices from MS to LS
			auto binding_iter = sorted_bindings.begin();
			const auto& binding_iter_end = sorted_bindings.end();

            // cur_slice = last-bound slice, starts just beyond the port's range
			int cur_slice = port.get_depth();

            while (cur_slice > 0)
            {
                // Get the next binding, if there is one
                const PortBinding* binding = 
                    (binding_iter == binding_iter_end)? nullptr : &(*binding_iter);

                // This is the topmost slice extent+1 of the next binding.
                // If there is no next binding, 0 makes the math work consistently.
                int binding_hi_slice = binding? 
                    (binding->get_port_lo_slice() + binding->get_bound_slices()) : 0;

                // Is there an unbound gap between cur_slice and binding_hi_slice?
                // Fill it with zs
                if (binding_hi_slice < cur_slice)
                {
                    assert(can_tie);
                    formatted_slices.push_back(format_unconn(port.get_width()));

                    // This processed only a single unconnected slice. We'll return here
                    // multiple times if there were multiple unconnected slices. Do not advance
                    // the binding iterator - we haven't gotten to that binding yet.
                    cur_slice--;
                }
                else
                {
                    // No unbound gap - must be at a binding
                    assert(binding);

                    // Does this binding bind entire slice(s)?
                    if (binding->is_full0_port_binding())
                    {
                        // Target name
                        std::string bnd = format_bindable(binding->get_target());

                        // If only using some slices of the target, do indexing on the outer dim
                        if (!binding->is_full1_target_binding())
                        {
                            int lo = binding->get_target_lo_slice();
                            int hi = lo + binding->get_bound_slices() - 1;
                            
                            bnd += "[" + std::to_string(hi) + ":" + std::to_string(lo) + "]";
                        }

                        formatted_slices.push_back(bnd);

                        // Advance to the next binding
                        ++binding_iter;
                    }
                    else
                    {
                        // Otherwise, this binding binds bit ranges within a slice.
                        // Hand off to function to process it, and advance binding_iter for us
                        assert(binding->get_bound_slices() == 1);

                        formatted_slices.push_back(format_port_bindings_bits(port, 
                            binding->get_target_lo_slice(), binding_iter, binding_iter_end));

                        // Do not increment binding iter - this was done for us in the function above
                    }

                    // We've bound at least one slice just now. Move cur_slice down by that amount
                    cur_slice -= binding->get_bound_slices();
                }
            } // while cur_slice > 0
		} // if (port is bound)
		
        // Collapse the vector into a string
        std::string result;

        for (auto it = formatted_slices.begin(); it != formatted_slices.end(); ++it)
        {
            // Precede with comma if not the first element
            if (it != formatted_slices.begin())
                result += ",";
            result += *it;
        }

        // Surround with braces if there was more than one comma-separated element
        if (formatted_slices.size() > 1)
            result = "{" + result + "}";

		return result;
	}

    std::string format_size(int depth, int width)
    {
        std::string size_str;

        if (depth > 1)
        {
            size_str += "[" + std::to_string(depth-1) + ":0]";
        }

        if (width > 1 || depth > 1)
        {
            size_str += "[" + std::to_string(width-1) + ":0] ";
        }

        return size_str;
    }

    std::string format_bits_val(const BitsVal& val)
    {
        std::string result;

        unsigned depth = val.get_size(1);
        unsigned width = val.get_size(0);

        // Verilog-specific
        if (depth > 1)
            result += "{";

        for (unsigned i = 0; i < depth; i++)
        {
            if (i > 0)
                result += ",";

            result += std::to_string(width);
            result += "'";

            switch (val.get_preferred_base())
            {
            case BitsVal::BIN: result += val.to_str_bin(i); break;
            case BitsVal::DEC: result += val.to_str_dec(i); break;
            case BitsVal::HEX: result += val.to_str_hex(i); break;
            }
        }

        if (depth > 1)
            result += "}";

        return result;
    }

    std::string format_unconn(int bits)
    {
        std::string result;

        // Try and make it pretty. There's two ways we can write a run of z's:
        // 1) 5'bzzzzz
        // 2) {5'{1'bz}}
        // So let's try both and print out the one that's shorter.
        std::string version1 =
            std::to_string(bits) + "'b" + std::string(bits, 'z');
        std::string version2 =
            "{" + std::to_string(bits) + "{1'bz}}";

        // Prefer the first one in case of a tie, just because
        result = version2.length() <= version1.length()? version2 : version1;

        return result;
    }

    std::string format_bindable(Bindable* b)
    {
        std::string result;

        // Check using RTTI what type it is. No virtual function polymorphism, to separate
        // HDL structures from verilog-specific formatting. TODO: fancy cleanup of this

        if (auto net = dynamic_cast<Net*>(b))
        {
            result = net->get_name();
        }
        else if (auto cv = dynamic_cast<ConstValue*>(b))
        {
            result = format_bits_val(cv->get_value());
        }
        else
        {
            assert(false);
        }

        return result;
    }

	void write_inst_portbindings(const Port& port)
	{
		const std::string& portname = port.get_name();
		std::string bindings = format_port_bindings(port);
		write_line("." + portname + "(" + bindings + ")", true, false);
	}

	void write_inst_parambinding(const std::string& name, NodeParam* param)
	{
		std::string paramval = format_param(param);
		write_line("." + name + "(" + paramval + ")", true, false);
	}

	void write_sys_insts(HDLState& mod)
	{
		auto sys = dynamic_cast<NodeSystem*>(mod.get_node());
        assert(sys);

		write_line("");
		
		auto nodes = sys->get_nodes();
		for (auto& node : nodes)
		{
			auto& vinfo = node->get_hdl_state();

			// Gather bound params only (ones with a value attached to them)
			auto& bound_params = node->get_params();

			// Write out parameter bindings for this instance
			bool has_params = !bound_params.empty();

			write_line(node->get_hdl_name() + " ", true, false);
			if (has_params)
			{
				write_line("#", false, true);
				write_line("(");
				s_cur_indent++;

				for (auto it = bound_params.begin(); it != bound_params.end(); ++it)
				{
                    if (it != bound_params.begin())
                        write_line(",", false, true);
					write_inst_parambinding(it->first, it->second);
				}

				write_line("", false, true);
				s_cur_indent--;
				write_line(")");
				write_line("", true, false);
			}

			// The instance name
			write_line(node->get_name(), false, true);
			write_line("(");
			s_cur_indent++;

			// Write port bindings
            auto& ports = vinfo.get_ports();
			for (auto it = ports.begin(); it != ports.end(); ++it)
			{
                if (it != ports.begin())
                    write_line(",", false, true);

                write_inst_portbindings(it->second);
			}

			write_line("", false, true);
			s_cur_indent--;
			write_line(");");
			write_line("");
		}
	}

	void write_sys_localparams(HDLState& mod)
	{
		// Write Params of the System that have concrete values bound already.
		auto node = mod.get_node();
		auto& params = node->get_params();

        // Vector of pairs from the unordered_map. Will copy only the bound params into here.
        using PairEntry = typename std::remove_reference<decltype(params)>::type::value_type;
        std::vector<PairEntry> bound_params;

        for (auto& entry : params)
        {
            if (entry.second->get_type() != NodeParam::SYS)
                bound_params.push_back(entry);
        }

		for (auto& i : bound_params)
		{
            write_sys_localparam(i.first, i.second);
		}

		write_line("");
	}

	void write_sys_body(HDLState& mod)
	{
		s_cur_indent++;

		write_sys_localparams(mod);
		write_sys_nets(mod);
		write_sys_insts(mod);
		write_sys_assigns(mod);

		s_cur_indent--;
	}

	void write_sys_file(HDLState& mod)
	{
		const std::string& mod_name = mod.get_node()->get_hdl_name();

		write_line("module " + mod_name, true, false);

		write_sys_params(mod);

		write_line("(");
		write_sys_ports(mod);
		write_line(");");

		write_sys_body(mod);

		write_line("endmodule");
	}
}

void genie::impl::hdl::write_system(NodeSystem* sys)
{
	auto& top = sys->get_hdl_state();
	std::string filename = sys->get_hdl_name() + ".sv";

    genie::log::info("Writing %s", filename.c_str());

	s_file.open(filename);
	write_sys_file(top);
	s_file.close();
}
