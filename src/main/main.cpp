#include <iostream>

#include "getoptpp/getopt_pp.h"
#include "globals.h"
#include "io.h"

#include "genie/genie.h"
#include "genie/net_clock.h"

using namespace genie;

namespace
{
	std::string s_script;

	void parse_args(int argc, char** argv)
	{
		GetOpt::GetOpt_pp args(argc, argv);

		//args >> GetOpt::Option('p', "component_path", Globals::inst()->component_path);

		//if (!(args >> GetOpt::GlobalOption(s_script)))
		//	throw Exception("Must specify script");
	}
}

int main(int argc, char** argv)
{
	try
	{
		System* sys = new System(argv[1]);
		genie::get_root()->add_object(sys);
	}
	catch (std::exception& e)
	{
		IO::msg_error(e.what());
	}



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
