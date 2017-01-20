#include "genie/genie.h"
#include "genie/node.h"
#include "lauxlib.h"
#include "lua_if.h"

using namespace genie;
using namespace genie::lua_if;

/// @module genie

namespace
{
    /// Represents a Verilog module, system, or module instance in a system
    ///
    /// @type Node

    /// Get the Node's name
    /// @function get_name
    /// @treturn string name
    LFUNC(node_get_name)
    {
        auto self = lua_if::check_object<Node>(1);
        
        lua_pushstring(L, self->get_name().c_str());
        return 1;
    }

    /// Set an integer parameter
    /// @function set_int_param
    /// @tparam string name Parameter name
    /// @tparam string expr Expression
    LFUNC(node_set_int_param)
    {
        auto self = lua_if::check_object<Node>(1);
        const char* name = luaL_checkstring(L, 2);
        const char* expr = luaL_checkstring(L, 3);

        self->set_int_param(name, expr);
        return 0;
    }

    /// Set string parameter
    /// @function set_str_param
    /// @tparam string name Parameter name
    /// @tparam string val Parameter value
    LFUNC(node_set_str_param)
    {
        auto self = lua_if::check_object<Node>(1);
        const char* name = luaL_checkstring(L, 2);
        const char* expr = luaL_checkstring(L, 3);

        self->set_str_param(name, expr);
        return 0;
    }

    /// Set literal parameter.
    ///
    /// A literal parameter's value is copied verbatim from the supplied
    /// string, without quotes. Useful for referencing system parameters.
    /// @function set_lit_param
    /// @tparam string name Parameter name
    /// @tparam string val Parameter value
    LFUNC(node_set_lit_param)
    {
        auto self = lua_if::check_object<Node>(1);
        const char* name = luaL_checkstring(L, 2);
        const char* expr = luaL_checkstring(L, 3);

        self->set_lit_param(name, expr);
        return 0;
    }

    LCLASS(Node,
    {
        LM(get_name, node_get_name),
        LM(set_int_param, node_set_int_param),
        LM(set_str_param, node_set_str_param),
        LM(set_lit_param, node_set_lit_param)
    });

    /// Represents a system
    ///
    /// A subclass of @{Node} capable of holding instances of @{Node}s and
    /// various connnections between them. GENIE's main output is the source 
    /// code for each System.
    /// @type System

    /// Create system parameter.
    ///
    /// Creates a top-level SystemVerilog parameter for the System that has no
    /// assigned value, but can be set later when the system is instantiated.
    /// System parameters can not participate in expressions, and their only 
    /// intended use is to be passed down to @{Node} instances within the @{System} 
    /// to be consumed as literal parameters.
    /// @see Node:set_lit_param
    /// @function create_sys_param
    /// @tparam string name Parameter name
    LFUNC(sys_create_sys_param)
    {
        auto self = lua_if::check_object<System>(1);
        const char* name = luaL_checkstring(L, 2);

        self->create_sys_param(name);
        return 0;
    }

    LSUBCLASS(System, (Node),
    {
        LM(create_sys_param, sys_create_sys_param)
    });
}

