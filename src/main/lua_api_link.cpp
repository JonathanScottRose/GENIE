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

	LSUBCLASS(LinkRS, (Link), {});

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