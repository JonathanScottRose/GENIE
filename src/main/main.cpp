#include <iostream>
#include "ct/common.h"
#include "getoptpp/getopt_pp.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"


#include "io.h"
#include "globals.h"

using namespace ct;


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

		lua_State *L = luaL_newstate();
		luaL_checkversion(L);
		luaL_openlibs(L);

		int s = luaL_loadfile(L, s_script.c_str());
		if (s != 0)
		{
			std::string err = lua_tostring(L, -1);
			lua_pop(L, 1);
			throw Exception(err);
		}
		s = lua_pcall(L, 0, LUA_MULTRET, 0);
		if (s != 0)
		{
			std::string err = lua_tostring(L, -1);
			lua_pop(L, 1);
			throw Exception(err);
		}

		lua_close(L);
	}
	catch (std::exception& e)
	{
		IO::msg_error(e.what());		
	}

	return 0;
}
