#include <iostream>

#include "getoptpp/getopt_pp.h"
#include "io.h"
#include "debugger.h"

#include "genie/genie.h"
#include "genie/lua/genie_lua.h"
#include "genie/regex.h"
#include "genie/log.h"

using namespace genie;

namespace
{
	std::string s_script;
	std::string s_lua_args;

	bool s_debug = false;
	std::string s_debug_host = "localhost";
	int s_debug_port = 8172;

	lua::ArgsVec parse_lua_args()
	{
		lua::ArgsVec args;

		// Extract key=val,key=val,... pairs from string
		for (auto cur_pos = s_lua_args.cbegin(), end_pos = s_lua_args.cend(); cur_pos != end_pos; )
		{
			static std::regex pattern(R"(((\w+)=([^,=]+)(,)?).*)");
			std::smatch mr;

			if (!std::regex_match(cur_pos, end_pos, mr, pattern))
				throw Exception("malformed Lua args: " + s_lua_args);

			args.emplace_back(std::make_pair(mr[2], mr[3]));
			cur_pos = mr[1].second;
		}

		return args;
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
        auto& opts = genie::options();
		GetOpt::GetOpt_pp args(argc, argv);
		
		args >> GetOpt::OptionPresent("dump_dot", opts.dump_dot);
		args >> GetOpt::Option("dump_dot", opts.dump_dot_network);
		args >> GetOpt::Option("args", s_lua_args);
		args >> GetOpt::OptionPresent("force_full_merge", opts.force_full_merge);
        args >> GetOpt::OptionPresent("no_mdelay", opts.no_mdelay);
        args >> GetOpt::OptionPresent("detailed_stats", opts.detailed_stats);
        args >> GetOpt::OptionPresent("descriptive_spmg", opts.desc_spmg);
        args >> GetOpt::Option("register_spmg", opts.register_spmg);
        
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
}

int main(int argc, char** argv)
{
	try
	{
		parse_args(argc, argv);

		genie::init();
		lua::init(parse_lua_args());		

		if (s_debug)
		{
			start_debugger(s_debug_host.c_str(), s_debug_port);
		}

		lua::exec_script(s_script);		

		flow_main();
	}
	catch (std::exception& e)
	{
        genie::log::error(e.what());
	}

	lua::shutdown();

	return 0;
}
