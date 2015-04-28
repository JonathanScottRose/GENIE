#pragma once

//
// Private implementations of templated things and data structures for genie_lua.h. Do not use directly.
//

#include "genie/common.h"
#include "genie/lua/lua.h"
#include "genie/lua/lauxlib.h"

namespace genie
{
namespace lua
{
	typedef std::vector<std::pair<const char*, lua_CFunction>> FuncList;
	typedef std::function<bool(const Object*)> RTTICheckFunc;
	typedef std::function<void(void)> RTTIThrowFunc;
	typedef std::function<bool(const RTTIThrowFunc&)> RTTICatchFunc;

	// Information for an exported C++ class
	struct ClassRegEntry
	{
		const char* name;
		FuncList methods;
		FuncList statics;
		RTTICheckFunc cfunc;
		RTTIThrowFunc throwfunc;
		RTTICatchFunc catchfunc;
	};

	// Templated version whose constructor sets up the structure for a specific C++ class
	template<class T>
	struct ClassRegEntryT : public ClassRegEntry
	{
		ClassRegEntryT(const char* _name, const FuncList& _methods, 
			const FuncList& _statics = FuncList())
		{
			name = _name;
			methods = _methods;
			statics = _statics;

			// Check func: returns whether o is castable to type T
			cfunc = [](const Object* o)
			{
				return dynamic_cast<const T*>(o) != nullptr;
			};

			// Throw func: throws an object of type T*
			throwfunc = []()
			{
				throw static_cast<T*>(nullptr);
			};

			// Catch func: returns whether the given throwfunc throws an object of type T*
			catchfunc = [](const RTTIThrowFunc& th)
			{
				try	{ th();	}
				catch(T*) { return true; }
				catch (...) {}

				return false;
			};
		}
	};

	// Staticinit registries for classes and globals, respectively
	typedef StaticInitBase<ClassRegEntry> ClassReg;
	typedef StaticInitBase<FuncList> GlobalsReg;

	namespace priv
	{
		Object* check_object(int narg);
	}

	template<class T>
	T* is_object(int narg)
	{
		Object* obj = priv::check_object(narg);
		T* result = as_a<T*>(obj);

		return result;
	}

	template<class T>
	T* check_object(int narg)
	{
		Object* obj = priv::check_object(narg);
		T* result = is_object<T>(narg);
		if (!result)
		{
			std::string msg = "can't convert to " + 
				obj_typename<T>() + " from " +
				obj_typename(obj);
			luaL_argerror(get_state(), narg, msg.c_str());
		}

		return result;
	}

	template<class T>
	std::string obj_typename(T* ptr)
	{
		if (ptr) return typeid(*ptr).name();
		else return typeid(T).name();
	}
}
}