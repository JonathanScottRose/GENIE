#include "lua_if.h"
#include "lauxlib.h"
#include "genie/genie.h"

namespace lua_api
{
	// Takes a Lua function argument at stack location narg,
	// and tries to parse it as a Hierarchy object. If it's
	// already an object in the form of userdata, it just returns it.
	// If the arg is a string, it tries to look up the full path in the 
	// hierarchy to find that object and then returns it.
	template<class T = genie::HierObject>
	T* check_obj_or_str_hierpath(lua_State* L, int narg, genie::HierObject* parent)
	{
		T* result = nullptr;

		// Check if it's a string first
		if (lua_isstring(L, narg))
		{
			const char* path = lua_tostring(L, narg);
			result = dynamic_cast<T*>(parent->get_child(path));
			if (!result)
			{
				std::string error("child object of type " +
					std::string(typeid(T).name()) + " does not exist");
				luaL_argerror(L, narg, error.c_str());
			}
		}
		else
		{
			result = lua_if::check_object<T>(narg);
		}

		return result;
	}
}