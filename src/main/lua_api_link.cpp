#include "genie/genie.h"
#include "genie/link.h"
#include "lauxlib.h"
#include "lua_if.h"
#include "util.h"

using namespace genie;
using namespace genie::lua_if;

/// @module genie

namespace
{
	/// Links between connectable objects.
	///
	/// @type Link

	LCLASS(Link, {});

	/// Routed Streaming links.
	///
	/// @type LinkRS

	/// Get the source address binding.
	///
	/// This is the address value, that when emitted by the link's source,
	/// selects this link. A `nil` value indicates that this link is unconditionally
	/// selected, regardless of source address.
	/// @function get_src_addr
	/// @treturn unsigned address in decimal, or nil
	LFUNC(linkrs_get_src_addr)
	{
		auto self = lua_if::check_object<LinkRS>(1);

		unsigned result = self->get_src_addr();
		if (result == LinkRS::ADDR_ANY)
			lua_pushnil(L);
		else
			lua_pushinteger(L, result);

		return 1;
	}

	/// Get the sink address binding.
	///
	/// This is the address value, that when received by the link's sink,
	/// identifies this link. A `nil` value indicates that no address value is 
	/// associated, at the sink, with this link.
	/// @function get_sink_addr
	/// @treturn unsigned address in decimal, or nil
	LFUNC(linkrs_get_sink_addr)
	{
		auto self = lua_if::check_object<LinkRS>(1);

		unsigned result = self->get_sink_addr();
		if (result == LinkRS::ADDR_ANY)
			lua_pushnil(L);
		else
			lua_pushinteger(L, result);

		return 1;
	}

	/// Set transmission packet size.
	///
	/// Defaults to 1 if unset. Overrides default packet size setting
	/// associated with the source RS port.
	/// @function set_packet_size
	/// @tparam unsigned size
	LFUNC(linkrs_set_packet_size)
	{
		auto self = lua_if::check_object<LinkRS>(1);
		lua_Integer size = luaL_checkinteger(L, 2);
		luaL_argcheck(L, size >= 1, 2, "packet size must be at least 1");

		self->set_packet_size((unsigned)size);

		return 0;
	}

	/// Set transmission importance.
	///
	/// Defaults to 1 if unset. Overrides default importance setting
	/// associated with the source RS port.
	/// @function set_importance
	/// @tparam float importance, between 0 and 1 inclusive
	LFUNC(linkrs_set_importance)
	{
		auto self = lua_if::check_object<LinkRS>(1);
		lua_Number imp = luaL_checknumber(L, 2);
		luaL_argcheck(L, imp >= 0 && imp <= 1, 2, "importance must be in the range [0,1]");

		self->set_importance((float)imp);

		return 0;
	}

	LSUBCLASS(LinkRS, (Link), 
	{
		LM(get_src_addr, linkrs_get_src_addr),
		LM(get_sink_addr, linkrs_get_sink_addr),
		LM(set_packet_size, linkrs_set_packet_size),
		LM(set_importance, linkrs_set_importance)
	});

	/// Topology links.
	///
	/// @type LinkTopo

	/// Set the minimum and/or maximum number of register stages on the link.
	///
	/// @function set_minmax_regs
	/// @tparam int min_regs minimum number of register stages, default 0
	/// @tparam[opt=nil] int max_regs maximum number of registers, default unbounded
	LFUNC(linktopo_set_minmax_regs)
	{
		auto self = lua_if::check_object<LinkTopo>(1);
		lua_Integer min_val = luaL_checkinteger(L, 2);
		lua_Integer max_val = luaL_optinteger(L, 3, LinkTopo::REGS_UNLIMITED);

		luaL_argcheck(L, min_val >= 0, 2, "min reg level must be nonnegative");
		luaL_argcheck(L, max_val >= 0, 3, "max reg level must be nonnegative");
		luaL_argcheck(L, max_val >= min_val, 2, "max regs must be at least min regs");

		self->set_minmax_regs((unsigned)min_val, (unsigned)max_val);

		return 0;
	}

	LSUBCLASS(LinkTopo, (Link), 
	{
		LM(set_minmax_regs, linktopo_set_minmax_regs)
	});

}