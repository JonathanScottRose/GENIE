#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "genie/lua/genie_lua.h"
#include "genie/common.h"

using namespace genie;
using namespace lua;

extern volatile int foo;

namespace
{
	const char* API_TABLE_NAME = "genie";
	lua_State* s_state = nullptr;
	std::vector<ClassRegEntry> s_reg_classes;

	int s_panic(lua_State* L)
	{
		std::string err = luaL_checkstring(L, -1);
		lua_pop(L, 1);
		throw Exception(err);
		return 0;
	}

	int s_stacktrace(lua_State* L)
	{
		const char* err = luaL_checkstring(L, -1);
		luaL_traceback(L, L, err, 1);
		return 1;
	}

	int s_printclass(lua_State* L)
	{
		lua_pushliteral(L, "userdata");
		return 1;
	}

	void s_register_funclist(const FuncList& flist)
	{
		for (const auto& entry : flist)
		{
			const char* name = entry.first;
			lua_CFunction func = entry.second;

			lua_pushcfunction(s_state, func);
			lua_setfield(s_state, -2, name);
		}
	}

	void s_register_class_cpp(const ClassRegEntry& entry)
	{
		// Check for duplicate entries
		std::for_each(s_reg_classes.begin(), s_reg_classes.end(),
			[&](const ClassRegEntry& other)
		{
			if (!strcmp(entry.name, other.name))
			{
				throw Exception("class " + std::string(entry.name) + " already registered");
			}
		});

		// Find out where to insert the new entry.
		// It should be inserted before the first class that is a superclass
		/*
		auto it = std::find_if(s_reg_classes.begin(), s_reg_classes.end(),
			[&](const ClassRegEntry& other)
		{
			return other.catchfunc(entry.throwfunc);
		});*/

		auto it = s_reg_classes.end();

		s_reg_classes.emplace(it, entry);
	}

	void s_register_class_lua(const ClassRegEntry& entry)
	{
		// Push API table on the stack
		lua_getglobal(s_state, API_TABLE_NAME);

		// Create class table
		lua_newtable(s_state);

		// Add static methods to the class table
		s_register_funclist(entry.statics);

		// Register class table in the API table, remove API table from stack
		lua_setfield(s_state, -2, entry.name);
		lua_pop(s_state, 1);

		// Create the registry-based metatable for this class. This will hold
		// all the instance methods, and the __index method that allows
		// instances of this class to automatically access those methods.
		if (luaL_newmetatable(s_state, entry.name) == 0)
			throw Exception("class name already exists: " + std::string(entry.name));

		// Set metatable's __index to point to the metatable itself.
		lua_pushvalue(s_state, -1);
		lua_setfield(s_state, -2, "__index");

		// Now add the instance methods
		s_register_funclist(entry.methods);

		// Instance metatable is all set up, we don't need it on the stack anymore.
		lua_pop(s_state, 1);
	}
}

//
// Public funcs
//

lua_State* lua::get_state()
{
	return s_state;
}

void lua::init()
{
	s_state = luaL_newstate();
	lua_atpanic(s_state, s_panic);
	luaL_checkversion(s_state);
	luaL_openlibs(s_state);

	lua_newtable(s_state);
	lua_setglobal(s_state, API_TABLE_NAME);

	for (auto& entry : ClassReg::entries())
	{
		s_register_class_lua(entry);
		s_register_class_cpp(entry);
	}
}

void lua::shutdown()
{
	if (s_state) lua_close(s_state);
}

void lua::exec_script(const std::string& filename)
{
	// Error handler functions for lua_pcall(). Push it before
	// luaL_loadfile pushes the loaded code onto the stack
	lua_pushcfunction(s_state, s_stacktrace);

	// Load the file. This pushes one entry on the stack.
	int s = luaL_loadfile(s_state, filename.c_str());
	if (s != LUA_OK)
	{
		std::string err = luaL_checkstring(s_state, -1);
		lua_pop(s_state, 1);
		throw Exception(err);
	}

	// Run the function at the top of the stack (the loaded file), using
	// the error handler function (which is one stack entry previous) if something
	// goes wrong.
	s = lua_pcall(s_state, 0, LUA_MULTRET, lua_absindex(s_state, -2));
	if (s != LUA_OK)
	{
		std::string err = luaL_checkstring(s_state, -1);
		lua_pop(s_state, 1);
		throw Exception(err);
	}
}

void lua::register_func(const char* name, lua_CFunction fptr)
{
	lua_getglobal(s_state, API_TABLE_NAME);
	lua_pushcfunction(s_state, fptr);
	lua_setfield(s_state, -2, name);
	lua_pop(s_state, 1);
}

void lua::register_funcs(luaL_Reg* entries)
{
	lua_getglobal(s_state, API_TABLE_NAME);
	luaL_setfuncs(s_state, entries, 0);
	lua_pop(s_state, 1);
}

int lua::make_ref()
{
	return luaL_ref(s_state, LUA_REGISTRYINDEX);
}

void lua::push_ref(int ref)
{
	lua_rawgeti(s_state, LUA_REGISTRYINDEX, ref);
}

void lua::free_ref(int ref)
{
	luaL_unref(s_state, LUA_REGISTRYINDEX, ref);
}

void lua::lerror(const std::string& what)
{
	lua_pushstring(s_state, what.c_str());
	lua_error(s_state);
}

void lua::push_object(Object* inst)
{
	// Associate correct metatable with it, based on RTTI type
	auto it = s_reg_classes.begin();
	auto it_end = s_reg_classes.end();
	for ( ; it != it_end; ++it)
	{
		if (it->cfunc(inst))
			break;
	}

	if (it == it_end)
	{
		lerror("C++ class not registered with Lua interface: " + 
			std::string(typeid(*inst).name()));
	}

	auto ud = (Object**)lua_newuserdata(s_state, sizeof(Object*));
	*ud = inst;

	luaL_setmetatable(s_state, it->name);
	
	// leaves userdata on stack
}

Object* lua::check_object_internal(int narg)
{
	auto ud = (Object**)lua_touserdata(s_state, narg);
	if (!ud)
		luaL_argerror(s_state, narg, "not userdata");

	return *ud;
}


