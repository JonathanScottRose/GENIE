#include "genie/genie.h"
#include "genie/port.h"
#include "lauxlib.h"
#include "lua_if.h"
#include "util.h"

using namespace genie;
using namespace genie::lua_if;


// Helper functions
namespace
{
    void parse_port_spec(lua_State* L, int narg, genie::HDLPortSpec& pspec)
    {
        if (lua_getfield(L, narg, "name") != LUA_TSTRING)
            luaL_argerror(L, narg, "port spec structure missing 'name' field");
        pspec.name = lua_tostring(L, -1);

        if (lua_getfield(L, narg, "width") != LUA_TNIL)
            pspec.width = lua_tostring(L, -1);
        else
            pspec.width = "1";

        if (lua_getfield(L, narg, "depth") != LUA_TNIL)
            pspec.depth = lua_tostring(L, -1);
        else
            pspec.depth = "1";

        lua_pop(L, 3);
    }

    void parse_bind_spec(lua_State* L, int narg, genie::HDLBindSpec& bspec)
    {
        bspec.depth = "1";
        bspec.width = "1";
        bspec.lo_bit = "0";
        bspec.lo_slice = "0";

        for (auto& p : {
            std::make_pair(&bspec.depth, "depth"), std::make_pair(&bspec.width, "width"),
            std::make_pair(&bspec.lo_bit, "lo_bit"), std::make_pair(&bspec.lo_slice, "lo_slice")})
        {
            auto& out = *(p.first);
            const char* field_name = p.second;

            if (lua_getfield(L, narg, field_name) != LUA_TNIL)
                out = lua_tostring(L, -1);

            lua_pop(L, 1);
        }
    }
}

/// @module genie

namespace
{
    /// Superclass of all GENIE Ports
    ///
    /// @type Port

    /// Get the Port's name
    /// @function get_name
    /// @treturn string name
    LFUNC(port_get_name)
    {
        auto self = lua_if::check_object<Port>(1);
        
        lua_pushstring(L, self->get_name().c_str());
        return 1;
    }

    /// Get the Port's direction
    /// @function get_dir
    /// @treturn string direction (IN/OUT)
    LFUNC(port_get_dir)
    {
        auto self = lua_if::check_object<Port>(1);
        const char* result = self->get_dir().to_string();
        assert(result);

        lua_pushstring(L, result);
        return 1;
    }

    LCLASS(Port,
    {
        LM(get_name, port_get_name)
    });

    /// Conduit Port.
    ///
    /// Used to connect bundles of HDL signals that have no interconnect-specific
    /// semantic meanings.
    /// @type PortConduit
    
    /// Add a signal to the Conduit Port.
    ///
    /// The signal roles available to Conduit Ports merely indicate
    /// the direction of the signal relative to the direction of the Conduit Port
    /// and otherwise have no semantic meaning. All roles require user-defined string
    /// tags to differentiate them.
    /// There are five possible signal roles for Conduit Ports:
    /// FWD, REV, IN, OUT, and INOUT.
    /// IN, OUT, and INOUT are absolute port directions.
    /// FWD and REV become OUT and IN if the PortConduit itself is an OUT port,
    /// and IN and OUT if the PortConduit is an IN port.
    /// @function add_signal
    /// @tparam string role FWD, REV, IN, OUT, or INOUT
    /// @tparam string tag unique user-defined tag
    /// @tparam string sig_name HDL signal name
    /// @tparam[opt='1'] string width width of the signal in bits (integer expression, may reference parameters), defaults to 1 bit
    LFUNC(conduitport_add_signal)
    {
        auto self = lua_if::check_object<PortConduit>(1);
		const char* role_str = luaL_checkstring(L, 2);
		const char* tag = "";
		const char* hdl_sig = "";
		const char* width_str = "1";

		switch (lua_gettop(L))
		{
		case 5: // self, role, tag, hdl_sig, width
			tag = luaL_checkstring(L, 3);
			hdl_sig = luaL_checkstring(L, 4);
			width_str = luaL_checkstring(L, 5);
			break;
		case 4: // self, role, hdl_sig, width
			width_str = luaL_checkstring(L, 4);
		case 3: // self, role, hdl_sig
			hdl_sig = luaL_checkstring(L, 3);
			break;
		default:
			luaL_error(L, "expected 2, 3, or 4 parameters");
		}

		auto role = SigRoleType::from_string(role_str);
		luaL_argcheck(L, role != SigRoleType::INVALID, 2, "invalid signal role");

		self->add_signal({ role, tag }, hdl_sig, width_str);
        return 0;
    }

    /// Add a signal to the Conduit Port.
    ///
    /// An advanced version of @{PortConduit:add_signal} to more finely control
    /// the size of the HDL port and the size of the binding to the HDL port.
    /// Expects two structures: a PortSpec and a BindSpec.
    /// The PortSpec structure shall be a table with the following fields:
    ///   name: the name of the HDL port
    ///   width: an expression specifying the width in bits of the HDL port
    ///   depth: an expression specifying the second size dimension of the HDL port
    /// Both width and depth are expressions that must evaluate to an integer and may contain
    /// references to @{Node} parameters. If either field is absent, it is defaulted to '1'.
    /// The BindSpec structure specifies the range of the HDL Port to bind to for this role.
    /// It shall be a table with the following fields:
    ///   lo_slice: the lower bound of the second dimension of the HDL port to bind to, default 0
    ///   slices: the number of second-dimension indicies to bind over, default 1
    ///   lo_bit: the least-significant bit to bind to, must be 0 if slices is > 1, default 0
    ///   width: the number of bits, starting from lo_bit, to bind, must be the entire port width
    ///          if slices > 1
    /// @function add_signal_ex
    /// @tparam string role FWD, REV< IN, OUT, or INOUT
    /// @tparam string tag unique user-defined tag
    /// @tparam table port_spec a PortSpec structure, as defined above
    /// @tparam table bind_spec a BindSpec structure, as defined above
    /// @see @{PortConduit:add_signal>
    LFUNC(conduitport_add_signal_ex)
    {
        auto self = lua_if::check_object<PortConduit>(1);
        const char* role_str = luaL_checkstring(L, 2);
        const char* tag = luaL_checkstring(L, 3);
        luaL_checktype(L, 4, LUA_TTABLE);
        luaL_checktype(L, 5, LUA_TTABLE);

        // Parse role
		auto role = SigRoleType::from_string(role_str);
		luaL_argcheck(L, role != SigRoleType::INVALID, 2, "invalid signal role");

        // Populate PortSpec struct
        genie::HDLPortSpec pspec;
        parse_port_spec(L, 4, pspec);

        // Populate BindSpec struct
        genie::HDLBindSpec bspec;
        parse_bind_spec(L, 5, bspec);

		self->add_signal_ex({ role, tag }, pspec, bspec);

        return 0;
    }

    LSUBCLASS(PortConduit, (Port), 
    {
        LM(add_signal, conduitport_add_signal),
        LM(add_signal_ex, conduitport_add_signal_ex)
    });

    /// Routed Streaming Port.
    ///
    /// Routed Streaming ports send data unidirectionally from one source to one or more sinks
    /// over an automatically-generated interconnect fabric. They implement flow control, unicast
    /// and multicast addressing, and variable-length packetization.
    /// @type PortRS

	/// Add a signal to the RS Port.
	///
	/// @function add_signal
	/// @tparam string role VALID, READY, DATA, DATABUNDLE, ADDR, EOP
	/// @tparam[opt] string tag unique user-defined tag for DATABUNDLE signals only
	/// @tparam string sig_name HDL signal name
	/// @tparam[opt='1'] string width width of the signal in bits (integer expression, may reference parameters), defaults to 1 bit, required if tag present
	LFUNC(rsport_add_signal)
	{
		auto self = lua_if::check_object<PortRS>(1);

		const char* role_str = luaL_checkstring(L, 2);
		const char* tag = "";
		const char* hdl_sig = "";
		const char* width_str = "1";
		
		switch (lua_gettop(L))
		{
		case 5: // self, role, tag, hdl_sig, width
			tag = luaL_checkstring(L, 3);
			hdl_sig = luaL_checkstring(L, 4);
			width_str = luaL_checkstring(L, 5);
			break;
		case 4: // self, role, hdl_sig, width
			width_str = luaL_checkstring(L, 4);
		case 3: // self, role, hdl_sig
			hdl_sig = luaL_checkstring(L, 3);
			break;
		default:
			luaL_error(L, "expected 2, 3, or 4 parameters");
		}

		auto role = SigRoleType::from_string(role_str);
		luaL_argcheck(L, role != SigRoleType::INVALID, 2, "invalid signal role");

		self->add_signal({ role, tag }, hdl_sig, width_str);
		return 0;
	}

	/// Add a signal to the RS Port.
	///
	/// An advanced version of @{PortRS:add_signal} to more finely control
	/// the size of the HDL port and the size of the binding to the HDL port.
	/// Expects two structures: a PortSpec and a BindSpec.
	/// The PortSpec structure shall be a table with the following fields:
	///   name: the name of the HDL port
	///   width: an expression specifying the width in bits of the HDL port
	///   depth: an expression specifying the second size dimension of the HDL port
	/// Both width and depth are expressions that must evaluate to an integer and may contain
	/// references to @{Node} parameters. If either field is absent, it is defaulted to '1'.
	/// The BindSpec structure specifies the range of the HDL Port to bind to for this role.
	/// It shall be a table with the following fields:
	///   lo_slice: the lower bound of the second dimension of the HDL port to bind to, default 0
	///   slices: the number of second-dimension indicies to bind over, default 1
	///   lo_bit: the least-significant bit to bind to, must be 0 if slices is > 1, default 0
	///   width: the number of bits, starting from lo_bit, to bind, must be the entire port width
	///          if slices > 1
	/// @function add_signal_ex
	/// @tparam string role FWD, REV< IN, OUT, or INOUT
	/// @tparam string tag unique user-defined tag
	/// @tparam table port_spec a PortSpec structure, as defined above
	/// @tparam table bind_spec a BindSpec structure, as defined above
	/// @see @{PortRS:add_signal>
	LFUNC(rsport_add_signal_ex)
	{
		auto self = lua_if::check_object<PortRS>(1);
		const char* role_str = luaL_checkstring(L, 2);
		const char* tag = luaL_checkstring(L, 3);
		luaL_checktype(L, 4, LUA_TTABLE);
		luaL_checktype(L, 5, LUA_TTABLE);

		// Parse role
		auto role = SigRoleType::from_string(role_str);
		luaL_argcheck(L, role != SigRoleType::INVALID, 2, "invalid signal role");

		// Populate PortSpec struct
		genie::HDLPortSpec pspec;
		parse_port_spec(L, 4, pspec);

		// Populate BindSpec struct
		genie::HDLBindSpec bspec;
		parse_bind_spec(L, 5, bspec);

		self->add_signal_ex({ role, tag }, pspec, bspec);

		return 0;
	}

	// Set inner logic depth.
	//
	// Specifies the number of logic levels behind this Port before seeing
	// a register. Default is 0.
	// @function set_logic_depth
	// @tparam unsigned depth
	LFUNC(rsport_set_logic_depth)
	{
		auto self = lua_if::check_object<PortRS>(1);
		auto depth = luaL_checkinteger(L, 2);
		luaL_argcheck(L, depth >= 0, 2, "logic depth must be nonnegative");

		self->set_logic_depth((unsigned)depth);
		return 0;
	}

	LSUBCLASS(PortRS, (Port),
	{
		LM(add_signal, rsport_add_signal),
		LM(add_signal_ex, rsport_add_signal_ex),
		LM(set_logic_depth, rsport_set_logic_depth)
	});
}

