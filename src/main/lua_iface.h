#pragma once

#include <string>
#include "ct/common.h"

struct lua_State;
typedef int (*lua_CFunction)(lua_State*);

namespace LuaIface
{
	void init();
	void shutdown();

	lua_State* get_state();
	void exec_script(const std::string& filename);
	void register_func(const std::string& name, lua_CFunction fptr);
	int make_ref();
	void push_ref(int ref);
	void free_ref(int ref);
}