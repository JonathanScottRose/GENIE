#include "genie/genie.h"
#include "genie/node.h"
#include "lua_api_common.h"
#include "lauxlib.h"
#include "lua_if.h"

using namespace genie;
using namespace genie::lua_if;

namespace
{
	/// Base class for all GENIE hierarchy objects.
	///
	/// @type HierObject

	/// Get the object's name
	/// @function get_name
	/// @treturn string name
	LFUNC(hier_get_name)
	{
		auto self = lua_if::check_object<HierObject>(1);

		lua_pushstring(L, self->get_name().c_str());
		return 1;
	}

	/// Get a handle to a child sub-object.
	///
	/// Throws an error if the object is not found.
	/// @function get_child
	/// @tparam string path dot-separated hierarchy path to child
	/// @treturn object
	LFUNC(hier_get_child)
	{
		auto self = lua_if::check_object<HierObject>(1);
		const char* path = luaL_checkstring(L, 2);

		auto obj = self->get_child(path);
		if (!obj)
		{
			luaL_error(L, "%s: child object %s not found",
				self->get_name().c_str(), path);
		}

		lua_if::push_object(obj);

		return 1;
	}

	LCLASS(HierObject,
	{
		LM(get_name, hier_get_name),
		LM(get_child, hier_get_child)
	});
}