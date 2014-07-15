#include <stdexcept>
#include <iostream>
#include "getoptpp/getopt_pp.h"
#include "globals.h"
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
		
		args >> GetOpt::OptionPresent("p2pdot", Globals::inst()->dump_p2p_graph);
		args >> GetOpt::OptionPresent("topodot", Globals::inst()->dump_topo_graph);

		if (!(args >> GetOpt::GlobalOption(s_script)))
			throw Exception("Must specify script");
	}
}



int main(int argc, char** argv)
{
	try
	{
		LuaIface::init();
		parse_args(argc, argv);
		LuaIface::exec_script(s_script);

		for (auto& i : Spec::systems())
		{
			Spec::System* spec_sys = i.second;

			if (Globals::inst()->dump_topo_graph)
				spec_sys->get_topology()->dump_graph(spec_sys->get_name() + "_topo.dot");

			P2P::System* p2p_sys = ct::build_system(spec_sys);

			if (Globals::inst()->dump_p2p_graph)
				p2p_sys->dump_graph();

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
