#pragma once

#include "genie/common.h"
#include "genie/lua/lua.h"
#include "genie/lua/lualib.h"
#include "genie/lua/lauxlib.h"

/*
// Forward declarations for Lua stuff, to avoid including their header files here
struct lua_State;
struct luaL_Reg;
typedef int (*lua_CFunction)(lua_State*);
*/

namespace genie
{
namespace lua
{
	#define LFUNC(name) int name(lua_State* L)
	#define LM(name,func) {#name, func}

	typedef std::vector<std::pair<const char*, lua_CFunction>> FuncList;
	typedef std::function<bool(const Object*)> RTTICheckFunc;

	struct ClassRegDB
	{
		struct Entry
		{
			const char* name;
			FuncList methods;
			FuncList statics;
			RTTICheckFunc cfunc;
		};
		
		typedef std::forward_list<Entry> Entries;

		static Entries& entries()
		{
			static Entries s_entries;
			return s_entries;
		}
	};

	template<class T>
	struct ClassReg
	{
		ClassReg(const char* name, const FuncList& methods, 
			const FuncList& statics = FuncList())
		{
			ClassRegDB::Entry entry;
			entry.name = name;
			entry.methods = methods;
			entry.statics = statics;
			entry.cfunc = [](const Object* o)
			{
				return dynamic_cast<const T*>(o) != nullptr;
			};

			ClassRegDB::entries().push_front(entry);
		}
	};

	// Init/shutdown
	void init();
	void shutdown();
	
	// Class registration and C++ interop
	void register_func(const char* name, lua_CFunction fptr);
	void register_funcs(struct luaL_Reg* entries);
	void push_object(Object* inst);

	template<class T> 
	T* check_object(int narg);

	// Utility
	lua_State* get_state();
	void exec_script(const std::string& filename);
	int make_ref();
	void push_ref(int ref);
	void free_ref(int ref);
	void lerror(const std::string& what);

	// Inline template code (ugly, exposes implementation)
	Object* check_object_internal(int narg);

	template<class T>
	T* check_object(int narg)
	{
		Object* obj = check_object_internal(narg);
		T* result = as_a<T*>(obj);
		if (!result)
		{
			std::string msg = "can't convert to " + 
				std::string(typeid(T).name()) + " from " +
				std::string(typeid(*obj).name());
			luaL_argerror(get_state(), narg, msg.c_str());
		}

		return result;
	}
}
}