#include <cassert>
#include "genie/genie.h"
#include "genie/log.h"
#include "lua.h"
#include "lauxlib.h"
#include "luasocket.h"
#include "lua_if.h"

using namespace genie;

namespace
{
	
}

void start_debugger(const char* host, int port)
{
	lua_State* L = lua_if::get_state();

	log::info("Connecting to remote debugger at %s:%d", host, port);

	// Load socket library for debugger
	luaL_requiref(L, "socket.core", luaopen_socket_core, 1);
	if (lua_isnil(L, 1))
		throw Exception("couldn't initialize socket library");

	// Parse code to: load mobdebug library and connect
	// This call itself shouldn't fail, as it's only parsing syntax
	std::string cmd = "return require('mobdebug').start('" +
		std::string(host) + "', " + std::to_string(port) + ")\n";
	int result = luaL_loadstring(L, cmd.c_str());
	assert(result == LUA_OK);

	// Execute the code, expect one return value
	// Will throw an exception if some bad things happen
	lua_if::pcall_top(0, 1);

	// If the call is successful, there might still be an error
	// in the form of a nil return value. Check for that.
	
	bool success = lua_toboolean(L, -1) != 0;
	lua_pop(L, 1);

	if (!success)
	{
		throw Exception("remote debug connection failed");
	}
}