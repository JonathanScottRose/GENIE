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
	typedef std::vector<std::pair<const char*, lua_CFunction>> FuncList;
	typedef std::function<bool(const Object*)> RTTICheckFunc;
	typedef std::function<void(void)> RTTIThrowFunc;
	typedef std::function<bool(const RTTIThrowFunc&)> RTTICatchFunc;

	struct ClassRegEntry
	{
		const char* name;
		FuncList methods;
		FuncList statics;
		RTTICheckFunc cfunc;
		RTTIThrowFunc throwfunc;
		RTTICatchFunc catchfunc;
	};

	template<class T>
	struct ClassRegEntryT : public ClassRegEntry
	{
		ClassRegEntryT(const char* _name, const FuncList& _methods, 
			const FuncList& _statics = FuncList())
		{
			name = _name;
			methods = _methods;
			statics = _statics;
			cfunc = [](const Object* o)
			{
				return dynamic_cast<const T*>(o) != nullptr;
			};
			throwfunc = []()
			{
				throw static_cast<T*>(nullptr);
			};
			catchfunc = [](const RTTIThrowFunc& th)
			{
				try	{ th();	}
				catch(T*) { return true; }
				catch (...) {}

				return false;
			};
		}
	};

	typedef StaticInitBase<ClassRegEntry> ClassReg;

	#define LFUNC(name) int name(lua_State* L)
	#define LM(name,func) {#name, func}
	#define LCLASS(cls, ...) ClassReg s_##cls##_reg(ClassRegEntryT<cls>(__VA_ARGS__))

	

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