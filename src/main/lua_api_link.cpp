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

	LSUBCLASS(LinkRS, (Link), 
	{
		LM(get_src_addr, linkrs_get_src_addr),
		LM(get_sink_addr, linkrs_get_sink_addr)
	});

	/// Topology links.
	///
	/// @type LinkTopo

	/// Set the minimum number of register stages.
	///
	/// @function set_min_regs
	/// @tparam int regs minimum number of register stages
	LFUNC(linktopo_set_min_regs)
	{
		auto self = lua_if::check_object<LinkTopo>(1);
		lua_Integer val = luaL_checkinteger(L, 2);

		luaL_argcheck(L, val >= 0, 2, "min reg level must be nonnegative");

		self->set_min_regs((unsigned)val);

		return 0;
	}

	LSUBCLASS(LinkTopo, (Link), 
	{
		LM(set_min_regs, linktopo_set_min_regs)
	});

}