#include <iostream>
#include <regex>


#include "getoptpp/getopt_pp.h"
#include "io.h"
#include "debugger.h"
#include "lua_if.h"

#include "genie/genie.h"
#include "genie/log.h"


using namespace genie;

namespace
{
	std::string s_script;
	lua_if::ArgsVec s_lua_args;
	genie::FlowOptions s_genie_opts;
	bool s_debug = false;
	std::string s_debug_host = "localhost";
	int s_debug_port = 8172;

	void parse_lua_args(const std::string& argstr)
	{
		// Extract key=val,key=val,... pairs from string
		for (auto cur_pos = argstr.cbegin(), end_pos = argstr.cend(); cur_pos != end_pos; )
		{
			static std::regex pattern(R"(((\w+)=([^,=]+)(,)?).*)");
			std::smatch mr;

			if (!std::regex_match(cur_pos, end_pos, mr, pattern))
				throw Exception("malformed Lua args: " + argstr);

			s_lua_args.emplace_back(std::make_pair(mr[2], mr[3]));
			cur_pos = mr[1].second;
		}
	}

	void parse_host_port(const std::string& str, std::string& out_host, int& out_port)
	{
		static std::regex pattern(R"(([^0-9:][^:]*)(:([0-9]+))?)");
		std::smatch mr;
		
		if (!std::regex_match(str.begin(), str.end(), mr, pattern))
			throw Exception("invalid host(:port) - " + str);

		if (mr[1].matched)
			out_host = mr[1];

		if (mr[3].matched)
			out_port = std::stoi(mr[3]);
	}

    std::vector<std::string> parse_list(const std::string& list)
    {
        std::vector<std::string> result;

        // Extract item,item,item... from string
        for (auto cur_pos = list.cbegin(), end_pos = list.cend(); cur_pos != end_pos; )
        {
            static std::regex pattern(R"(((\w+)(,)?).*)");
            std::smatch mr;

            if (!std::regex_match(cur_pos, end_pos, mr, pattern))
                break;

            result.emplace_back(mr[2]);
            cur_pos = mr[1].second;
        }
        
        return result;
    }

	void parse_args(int argc, char** argv)
	{
        auto& opts = s_genie_opts;
		GetOpt::GetOpt_pp args(argc, argv);
		
		args >> GetOpt::OptionPresent("dump_dot", opts.dump_dot);
		args >> GetOpt::Option("dump_dot", opts.dump_dot_network);
		args >> GetOpt::OptionPresent("force_full_merge", opts.force_full_merge);
        args >> GetOpt::OptionPresent("no_mdelay", opts.no_mdelay);
        args >> GetOpt::OptionPresent("detailed_stats", opts.detailed_stats);
        args >> GetOpt::OptionPresent("descriptive_spmg", opts.desc_spmg);
        args >> GetOpt::Option("register_spmg", opts.register_spmg);
        
		{
			std::string argstr;
			args >> GetOpt::Option("args", argstr);
			parse_lua_args(argstr);
		}

		args >> GetOpt::OptionPresent("debug", s_debug);
		if (s_debug)
		{
			std::string hostport;
			args >> GetOpt::Option("debug", hostport);
			if (!hostport.empty())
				parse_host_port(hostport, s_debug_host, s_debug_port);
		}

        {
            std::string no_topo_opt_sys;
            args >> GetOpt::OptionPresent("topo_opt", opts.topo_opt);
            args >> GetOpt::Option("topo_opt", no_topo_opt_sys);
            opts.topo_opt_systems = parse_list(no_topo_opt_sys);
        }

		if (!(args >> GetOpt::GlobalOption(s_script)))
			throw Exception("Must specify Lua script");
	}

    void s_exec_script()
    {
        log::info("Executing script %s", s_script.c_str());

        if (!s_lua_args.empty())
        {
            log::info("Script args:");
            for (const auto& parm : s_lua_args)
            {
                log::info("  %s=%s", parm.first.c_str(), parm.second.c_str());
            }
        }

        lua_if::exec_script(s_script);	
    }
}

int main(int argc, char** argv)
{
	try
	{
		parse_args(argc, argv);

		genie::init(&s_genie_opts);
		lua_if::init(s_lua_args);

		if (s_debug)
		{
			start_debugger(s_debug_host.c_str(), s_debug_port);
		}

        s_exec_script();

		genie::do_flow();
	}
	catch (std::exception& e)
	{
        genie::log::error(e.what());
	}

	lua_if::shutdown();

	return 0;
}
