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

    /// Create a clock port.
    ///
    /// @function create_clock_port
    /// @tparam string name Port name
    /// @tparam string dir Direction (in/out)
    /// @tparam string sig HDL signal name
    LFUNC(node_create_clock_port)
    {
        auto self = lua_if::check_object<Node>(1);
        const char* name = luaL_checkstring(L, 2);
        const char* dir_str = luaL_checkstring(L, 3);
        const char* sig = luaL_checkstring(L, 4);

        Port::Dir dir;
        bool valid = Port::Dir::from_string(dir_str, dir);
        if (!valid)
            luaL_argerror(L, 3, "invalid port direction");

        auto result = self->create_clock_port(name, dir, sig);
        lua_if::push_object(result);
        return 1;
    }

    /// Create a reset port.
    ///
    /// @function create_reset_port
    /// @tparam string name Port name
    /// @tparam string dir Direction (in/out)
    /// @tparam string sig HDL signal name
    LFUNC(node_create_reset_port)
    {
        auto self = lua_if::check_object<Node>(1);
        const char* name = luaL_checkstring(L, 2);
        const char* dir_str = luaL_checkstring(L, 3);
        const char* sig = luaL_checkstring(L, 4);

        Port::Dir dir;
        bool valid = Port::Dir::from_string(dir_str, dir);
        if (!valid)
            luaL_argerror(L, 3, "invalid port direction");

        auto result = self->create_reset_port(name, dir, sig);
        lua_if::push_object(result);
        return 1;
    }

    /// Create a conduit port.
    ///
    /// @function create_conduit_port
    /// @tparam string name Port name
    /// @tparam string dir Direction (in/out)
    LFUNC(node_create_conduit_port)
    {
        auto self = lua_if::check_object<Node>(1);
        const char* name = luaL_checkstring(L, 2);
        const char* dir_str = luaL_checkstring(L, 3);

        Port::Dir dir;
        bool valid = Port::Dir::from_string(dir_str, dir);
        if (!valid)
            luaL_argerror(L, 3, "invalid port direction");

        auto result = self->create_conduit_port(name, dir);
        lua_if::push_object(result);
        return 1;
    }

    /// Create a Routed Streaming port.
    ///
    /// @function create_rs_port
    /// @tparam string name Port name
    /// @tparam string dir Direction (in/out)
    /// @tparam string clk_name Name of associated clock port on same Node
    LFUNC(node_create_rs_port)
    {
        auto self = lua_if::check_object<Node>(1);
        const char* name = luaL_checkstring(L, 2);
        const char* dir_str = luaL_checkstring(L, 3);
        const char* clk_name = luaL_checkstring(L, 4);

        Port::Dir dir;
        bool valid = Port::Dir::from_string(dir_str, dir);
        if (!valid)
            luaL_argerror(L, 3, "invalid port direction");

        auto result = self->create_rs_port(name, dir, clk_name);
        lua_if::push_object(result);
        return 1;
    }

    LCLASS(Node,
    {
        LM(get_name, node_get_name),
        LM(set_int_param, node_set_int_param),
        LM(set_str_param, node_set_str_param),
        LM(set_lit_param, node_set_lit_param),
        LM(create_clock_port, node_create_clock_port),
        LM(create_reset_port, node_create_reset_port),
        LM(create_conduit_port, node_create_conduit_port),
        LM(create_rs_port, node_create_rs_port)
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

