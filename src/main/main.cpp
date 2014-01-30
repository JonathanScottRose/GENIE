#include <stdexcept>
#include <iostream>
#include "getoptpp/getopt_pp.h"
#include "globals.h"
#include "build_spec.h"
#include "io.h"
#include "ct/ct.h"
#include "write_vlog.h"
#include "impl_vlog.h"
#include "lua_iface.h"

namespace
{
	std::string s_script;

	void parse_args(int argc, char** argv)
	{
		GetOpt::GetOpt_pp args(argc, argv);

		//args >> GetOpt::Option('p', "component_path", Globals::inst()->component_path);

		if (!(args >> GetOpt::GlobalOption(s_script)))
			throw Exception("Must specify script");
	}
}



int main(int argc, char** argv)
{
	try
	{
		parse_args(argc, argv);
		LuaIface::init();
		LuaIface::exec_script(s_script);
		
		auto elab_order = Spec::get_elab_order();

		for (auto spec_sys : elab_order)
		{
			P2P::System* p2p_sys = ct::build_system(spec_sys);
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

	LuaIface::shutdown();

	return 0;
}
