#include <iostream>

#include "getoptpp/getopt_pp.h"
#include "io.h"

#include "genie/genie.h"
#include "genie/lua/genie_lua.h"
#include "genie/regex.h"
#include "genie/log.h"

using namespace genie;

namespace
{
	std::string s_script;
	std::string s_lua_args;

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
