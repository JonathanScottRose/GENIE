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

	LFUNC(s_panic)
	{
		std::string err = luaL_checkstring(L, -1);
		throw Exception(err);
		return 0;
	}

	LFUNC(s_stacktrace)
	{
		const char* err = luaL_checkstring(L, -1);
		luaL_traceback(L, L, err, 1);
		return 1;
	}

	LFUNC(s_compareobj)
	{
		auto pa = (const void**)lua_topointer(L, 1);
		auto pb = (const void**)lua_topointer(L, 2);
		lua_pushboolean(L, *pa < *pb? 1 : 0);
		return 1;
	}

	LFUNC(s_equalobj)
	{
		auto pa = (const void**)lua_topointer(L, 1);
		auto pb = (const void**)lua_topointer(L, 2);
		lua_pushboolean(L, *pa == *pb? 1 : 0);
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
		
		auto it = std::find_if(s_reg_classes.begin(), s_reg_classes.end(),
			[&](const ClassRegEntry& other)
		{
			return other.catchfunc(entry.throwfunc);
		});

		//auto it = s_reg_classes.end();

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

		// Add a __classname string with the class's name, which might be useful somehow
		lua_pushstring(s_state, entry.name);
		lua_setfield(s_state, -2, "__classname");

		// Add __lt and __eq metafunctions to enable ordering/comparison
		lua_pushcfunction(s_state, s_compareobj);
		lua_setfield(s_state, -2, "__lt");

		lua_pushcfunction(s_state, s_equalobj);
		lua_setfield(s_state, -2, "__eq");

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

	// Push API table on the stack
	lua_getglobal(s_state, API_TABLE_NAME);

	for (auto& entry : GlobalsReg::entries())
	{
		s_register_funclist(entry);
	}

	lua_pop(s_state, 1);
}

void lua::shutdown()
{
	if (s_state) lua_close(s_state);
}

void lua::pcall_top(int nargs, int nret)
{
	// Calls the function at the top of the stack.
	
	// First, we insert an element at the bottom of the stack,
	// that element being a reference to the stacktrace function.
	lua_pushcfunction(s_state, s_stacktrace);
	lua_insert(s_state, 1);

	// Top of stack now has function plus args like before.
	// Call the function at the top. Error handler function is at index 1.
	int s = lua_pcall(s_state, nargs, nret, 1);
	if (s != LUA_OK)
	{
		std::string err = luaL_checkstring(s_state, -1);
		lua_pop(s_state, 1);
		lua_remove(s_state, 1);
		throw Exception(err);
	}

	// Remove stacktrace function.
	lua_remove(s_state, 1);
}

void lua::exec_script(const std::string& filename)
{
	// Load the file. This pushes one entry on the stack.
	int s = luaL_loadfile(s_state, filename.c_str());
	if (s != LUA_OK)
	{
		std::string err = luaL_checkstring(s_state, -1);
		lua_pop(s_state, 1);
		throw Exception(err);
	}

	// Run the function at the top of the stack (the loaded file)
	pcall_top(0, 0);	
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
	// If null, push nil
	if (!inst)
	{
		lua_pushnil(s_state);
		return;
	}

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

	auto ud = (Object**)lua_newuserdata(s_state, sizeof(Object*), 1);
	*ud = inst;

	luaL_setmetatable(s_state, it->name);
	
	// leaves userdata on stack
}

Object* lua::priv::check_object(int narg)
{
	auto ud = (Object**)lua_touserdata(s_state, narg);
	if (!ud)
		luaL_argerror(s_state, narg, "not userdata");

	return *ud;
}


