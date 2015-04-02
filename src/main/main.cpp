#include <iostream>

#include "getoptpp/getopt_pp.h"
#include "globals.h"
#include "io.h"

#include "genie/genie.h"
#include "genie/net_clock.h"
#include "genie/lua/genie_lua.h"
#include "genie/vlog.h"
#include "flow.h"

using namespace genie;

namespace
{
	std::string s_script;

	void parse_args(int argc, char** argv)
	{
		GetOpt::GetOpt_pp args(argc, argv);

		//args >> GetOpt::Option('p', "component_path", Globals::inst()->component_path);
		
		args >> GetOpt::OptionPresent("dump_dot", Globals::inst()->dump_dot);
		args >> GetOpt::Option("dump_dot", Globals::inst()->dump_dot_network);

		if (!(args >> GetOpt::GlobalOption(s_script)))
			throw Exception("Must specify Lua script");
	}

	void do_flow()
	{
		auto systems = genie::get_root()->get_systems();

		for (auto sys : systems)
		{
			flow_refine_rs(sys);
			flow_refine_topo(sys);
			vlog::flow_process_system(sys);

			if (Globals::inst()->dump_dot)
			{
				auto network = Network::get(Globals::inst()->dump_dot_network);
				if (!network)
					throw Exception("Unknown network " + Globals::inst()->dump_dot_network);

				sys->write_dot(sys->get_name() + "." + network->get_name(), network->get_id());
			}
		}
	}
}

int main(int argc, char** argv)
{
	try
	{
		parse_args(argc, argv);

		genie::init();
		lua::init();
		
		lua::exec_script(s_script);		

		do_flow();
	}
	catch (std::exception& e)
	{
		io::msg_error(e.what());
	}

	lua::shutdown();

	/*
	try
	{
		LuaIface::init();
		parse_args(argc, argv);
		LuaIface::exec_script(s_script);

		for (auto& i : Spec::systems())
		{
			Spec::System* spec_sys = i.second;
			P2P::System* p2p_sys = genie::build_system(spec_sys);
			Vlog::SystemModule* sys_mod = ImplVerilog::build_top_module(p2p_sys);
			WriteVerilog::go(sys_mod);
			
			delete sys_mod;
			delete p2p_sys;
		}
	}
	catch (std::exception& e)
	{
		IO::msg_error(e.what());		
	}

	LuaIface::shutdown();*/

	return 0;
}
