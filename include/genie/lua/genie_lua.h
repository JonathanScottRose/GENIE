#pragma once

#include <string>

// Forward declarations for Lua stuff, to avoid including their header files here
struct lua_State;
typedef int (*lua_CFunction)(lua_State*);

namespace genie
{
namespace lua
{
	void init();
	void shutdown();

	lua_State* get_state();
	void exec_script(const std::string& filename);
	void register_func(const char* name, lua_CFunction fptr);
	void register_funcs(struct luaL_Reg* entries);
	int make_ref();
	void push_ref(int ref);
	void free_ref(int ref);
}
}