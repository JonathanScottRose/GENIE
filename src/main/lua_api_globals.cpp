#include "genie/genie.h"
#include "genie/node.h"
#include "lauxlib.h"
#include "lua_if.h"

using namespace genie;
using namespace genie::lua_if;

// LDoc package description

/// Global functions and classes provided natively by the GENIE executable. 
/// The contents of this package come pre-loaded into the global environment in a table called 'genie'.
/// @module genie

namespace
{
    /// Create a new module
    /// @function create_module
    /// @tparam string name Module name
    /// @tparam[opt=name] string hdl_name Name of verilog module, defaults to same as module name
    /// @treturn Node
    LFUNC(genie_create_module)
    {
        const char* name = luaL_checkstring(L, 1);
        const char* hdl_name = name;
        if (!lua_isnoneornil(L, 2))
            hdl_name = luaL_checkstring(L, 2);

        auto result = genie::create_module(name, hdl_name);
        lua_if::push_object(result);

        return 1;
    }

    /// Create a new system
    /// @function create_system
    /// @tparam string name System name
    /// @treturn Node
    LFUNC(genie_create_system)
    {
        const char* name = luaL_checkstring(L, 1);

        auto result = genie::create_system(name);
        lua_if::push_object(result);

        return 1;
    }

    LGLOBALS(
    {
        LM(create_module, genie_create_module),
        LM(create_system, genie_create_system)
    });

   
}
