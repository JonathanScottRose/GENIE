#include "genie/genie.h"
#include "genie/node.h"
#include "lua_api_common.h"
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
    /// @tparam[opt] string sig HDL signal name, defaults to port name
    LFUNC(node_create_clock_port)
    {
        auto self = lua_if::check_object<Node>(1);
        const char* name = luaL_checkstring(L, 2);
        const char* dir_str = luaL_checkstring(L, 3);
		const char* sig = luaL_optstring(L, 4, name);

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
    /// @tparam[opt] string sig HDL signal name, defaults to port name
    LFUNC(node_create_reset_port)
    {
        auto self = lua_if::check_object<Node>(1);
        const char* name = luaL_checkstring(L, 2);
        const char* dir_str = luaL_checkstring(L, 3);
		const char* sig = luaL_optstring(L, 4, name);

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

	/// Create a clock link.
	///
	/// @function create_clock_link
	/// @tparam string|HierObject source hierarchy path or reference to source object
	/// @tparam string|HierObject sink hierarchy path or reference to sink object
	/// @treturn Link
	LFUNC(sys_create_clock_link)
	{
		System* self = lua_if::check_object<System>(1);

		auto src = lua_api::check_obj_or_str_hierpath<HierObject>(L, 2, self);
		auto sink = lua_api::check_obj_or_str_hierpath<HierObject>(L, 3, self);

		Link* link = self->create_clock_link(src, sink);
		lua_if::push_object(link);

		return 1;
	}

	/// Create a reset link.
	///
	/// @function create_reset_link
	/// @tparam string|HierObject source hierarchy path or reference to source object
	/// @tparam string|HierObject sink hierarchy path or reference to sink object
	/// @treturn Link
	LFUNC(sys_create_reset_link)
	{
		System* self = lua_if::check_object<System>(1);
		auto src = lua_api::check_obj_or_str_hierpath<HierObject>(L, 2, self);
		auto sink = lua_api::check_obj_or_str_hierpath<HierObject>(L, 3, self);

		Link* link = self->create_reset_link(src, sink);
		lua_if::push_object(link);

		return 1;
	}

	/// Create a conduit link.
	///
	/// @function create_conduit_link
	/// @tparam string|HierObject source hierarchy path or reference to source object
	/// @tparam string|HierObject sink hierarchy path or reference to sink object
	/// @treturn Link
	LFUNC(sys_create_conduit_link)
	{
		System* self = lua_if::check_object<System>(1);
		auto src = lua_api::check_obj_or_str_hierpath<HierObject>(L, 2, self);
		auto sink = lua_api::check_obj_or_str_hierpath<HierObject>(L, 3, self);

		Link* link = self->create_conduit_link(src, sink);
		lua_if::push_object(link);

		return 1;
	}

	/// Create an RS logical link.
	///
	/// The link may optionally be bound to an address at the source and/or the sink.
	/// An address is the value, that when emitted by the source or sink RS port's `address`
	/// signal, selects or identifies this link. To select a destination for a transmission,
	/// one would set the source address binding. A `nil` value indicates an unconditional
	/// selection, independent of the existence of an address.
	/// @function create_rs_link
	/// @tparam string|HierObject source hierarchy path or reference to source object
	/// @tparam string|HierObject sink hierarchy path or reference to sink object
	/// @tparam[opt=nil] unsigned src_addr source address binding, optional
	/// @tparam[opt=nil] unsigned sink_addr sink address binding, optional
	/// @treturn LinkRS
	LFUNC(sys_create_rs_link)
	{
		System* self = lua_if::check_object<System>(1);
		auto src = lua_api::check_obj_or_str_hierpath<HierObject>(L, 2, self);
		auto sink = lua_api::check_obj_or_str_hierpath<HierObject>(L, 3, self);

		unsigned src_addr = (unsigned)luaL_optinteger(L, 4, LinkRS::ADDR_ANY);
		unsigned sink_addr = (unsigned)luaL_optinteger(L, 5, LinkRS::ADDR_ANY);

		Link* link = self->create_rs_link(src, sink, src_addr, sink_addr);
		lua_if::push_object(link);

		return 1;
	}

	/// Create a topology link.
	///
	/// @function create_topo_link
	/// @tparam string|HierObject source hierarchy path or reference to source object
	/// @tparam string|HierObject sink hierarchy path or reference to sink object
	/// @treturn Link
	LFUNC(sys_create_topo_link)
	{
		System* self = lua_if::check_object<System>(1);
		auto src = lua_api::check_obj_or_str_hierpath<HierObject>(L, 2, self);
		auto sink = lua_api::check_obj_or_str_hierpath<HierObject>(L, 3, self);

		Link* link = self->create_topo_link(src, sink);
		lua_if::push_object(link);

		return 1;
	}

	/// Add a module instance to the System.
	///
	/// @function create_instance
	/// @tparam string mod_name name of module
	/// @tparam string inst_name name of instance
	/// @treturn Node
	LFUNC(sys_create_instance)
	{
		System* self = lua_if::check_object<System>(1);
		const char* mod_name = luaL_checkstring(L, 2);
		const char* inst_name = luaL_checkstring(L, 3);
		
		auto inst = self->create_instance(mod_name, inst_name);
		lua_if::push_object(inst);
		return 1;
	}

	/// Export a port belonging to a contained instance.
	///
	/// @function export_port
	/// @tparam string|HierObject port hierarchical path, relative to system, of port to export, or object handle
	/// @tparam[opt] string name name of exported port to create, defaults to autogenerated
	/// @treturn Port
	LFUNC(sys_export_port)
	{
		System* self = lua_if::check_object<System>(1);
		auto port = lua_api::check_obj_or_str_hierpath<Port>(L, 2, self);
		const char* opt_name = luaL_optstring(L, 3, "");

		auto result = self->export_port(port, opt_name);
		lua_if::push_object(result);
		return 1;
	}

	/// Add a Split node to the system.
	///
	/// @function create_split
	/// @tparam[opt] string name optional explicit name
	/// @treturn Node
	LFUNC(sys_create_split)
	{
		System* self = lua_if::check_object<System>(1);
		const char* opt_name = luaL_optstring(L, 2, "");

		auto result = self->create_split(opt_name);
		lua_if::push_object(result);
		return 1;
	}

	/// Add a Merge node to the system.
	///
	/// @function create_merge
	/// @tparam[opt] string name optional explicit name
	/// @treturn Node
	LFUNC(sys_create_merge)
	{
		System* self = lua_if::check_object<System>(1);
		const char* opt_name = luaL_optstring(L, 2, "");

		auto result = self->create_merge(opt_name);
		lua_if::push_object(result);
		return 1;
	}

	/// Marks a set of RS Links as temporally exclusive
	/// @function make_exclusive
	/// @tparam array(Link)|Set(Link) links Array or Set of RS Links
	LFUNC(sys_make_exclusive)
	{
		auto self = lua_if::check_object<System>(1);
		auto links = lua_api::get_array_or_set<LinkRS>(L, 2);
		self->make_exclusive(links);
		return 0;
	}

    LSUBCLASS(System, (Node),
    {
        LM(create_sys_param, sys_create_sys_param),
		LM(create_clock_link, sys_create_clock_link),
		LM(create_reset_link, sys_create_reset_link),
		LM(create_conduit_link, sys_create_conduit_link),
		LM(create_rs_link, sys_create_rs_link),
		LM(create_topo_link, sys_create_topo_link),
		LM(create_instance, sys_create_instance),
		LM(export_port, sys_export_port),
		LM(create_split, sys_create_split),
		LM(create_merge, sys_create_merge),
		LM(make_exclusive, sys_make_exclusive)
    });

	/// Represents a user-defined module.
	///
	/// A subclass of @{Node} that's returned by @{genie_create_module}.
	/// @type Module

	/// Create an internal link.
	///
	/// Informs GENIE of a semantic relationship between input and output RS ports
	/// of a user-defined module.
	/// @function create_internal_link
	/// @tparam string|PortRS source hierarchy path or reference to source object
	/// @tparam string|PortRS sink hierarchy path or reference to sink object
	/// @tparam int latency latency in clock cycles
	/// @treturn Link
	LFUNC(module_create_internal_link)
	{
		Module* self = lua_if::check_object<Module>(1);
		auto src = lua_api::check_obj_or_str_hierpath<PortRS>(L, 2, self);
		auto sink = lua_api::check_obj_or_str_hierpath<PortRS>(L, 3, self);
		unsigned latency = (unsigned)luaL_checkinteger(L, 4);

		Link* link = self->create_internal_link(src, sink, latency);
		lua_if::push_object(link);

		return 1;
	}

	LSUBCLASS(Module, (Node),
	{
		LM(create_internal_link, module_create_internal_link)
	});
}

