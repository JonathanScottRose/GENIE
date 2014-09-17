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

	std::vector<std::string> split(const std::string &s, char delim)
	{
		std::vector<std::string> elems;
		std::stringstream ss(s);
		std::string item;
		while (std::getline(ss, item, delim))
		{
			elems.push_back(item);
		}
		return elems;
	}

	void parse_params(const std::string& params)
	{
		auto keyvals = split(params, ',');
		for (auto& i : keyvals)
		{
			auto key_and_val = split(i, '=');
			if (key_and_val.size() != 2)
				throw Exception("Global parameter must have one key and one value: " + i);

			auto& key = key_and_val[0];
			auto& val = key_and_val[1];
			Globals::inst()->global_params[key] = val;
		}
	}

	void parse_args(int argc, char** argv)
	{
		GetOpt::GetOpt_pp args(argc, argv);

		//args >> GetOpt::Option('p', "component_path", Globals::inst()->component_path);
		
		args >> GetOpt::OptionPresent("p2pdot", Globals::inst()->dump_p2p_graph);
		args >> GetOpt::OptionPresent("topodot", Globals::inst()->dump_topo_graph);
		args >> GetOpt::OptionPresent("reg_merge", Globals::inst()->register_merge);

		std::string params;
		args >> GetOpt::Option("params", params);
		parse_params(params);

		if (!(args >> GetOpt::GlobalOption(s_script)))
			throw Exception("Must specify script");
	}

	void set_global_params()
	{
		for (auto& keyval : Globals::inst()->global_params)
		{
			LuaIface::set_global_param(keyval.first, keyval.second);
		}
	}
}



int main(int argc, char** argv)
{
	try
	{
		LuaIface::init();
		parse_args(argc, argv);
		set_global_params();
		LuaIface::exec_script(s_script);

		ct::get_opts().register_merge = Globals::inst()->register_merge;

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
