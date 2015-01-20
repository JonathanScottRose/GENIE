#include "genie/lua/genie_lua.h"
#include "genie/genie.h"
#include "genie/structure.h"
#include "genie/hierarchy.h"
#include "genie/connections.h"

using namespace genie;
using namespace lua;

namespace
{
	//
	// Helpers
	//

	// Takes a Lua function argument at stack location narg,
	// and tries to parse it as a Hierarchy object. If it's
	// already an object in the form of userdata, it just returns it.
	// If the arg is a string, it tries to look up the full path in the 
	// hierarchy to find that object and then returns it.
	template<class T = HierObject>
	T* check_obj_or_str_hierpath(lua_State* L, int narg, HierObject* parent = nullptr)
	{
		T* result = nullptr;
		parent = parent? parent : genie::get_root();

		// Check if it's a string first
		if (lua_isstring(L, narg))
		{
			const char* path = lua_tostring(L, narg);
			
			auto ap = parent->asp_get<AHierParent>();
			if (!ap)
				throw Exception("not a hierarchy parent");

			result = as_a<T*>(ap->get_child(path));
		}
		else
		{
			result = lua::check_object<T>(narg);
		}

		return result;
	}

	// Pushes an array of things onto the stack.
	// In Lua, this is just a table with the keys being 1, 2, 3, etc
	// and the values being the array values.
	// Pushes onto the table at the top of the stack.
	// Returns the last index pushed + 1.
	// Optionally takes in the first index to start with.
	// The return value plus optional argument can be used to call this
	// multiple times to concatenate to the same array.
	template<class T>
	int push_array(lua_State* L, const T& arr, unsigned int index = 1)
	{
		for (auto& item : arr)
		{
			lua_pushunsigned(L, index);
			lua::push_object(item);
			lua_settable(L, -3);
			index++;
		}

		return index;
	}

	//
	// HierObject (re-used for many classes)
	//

	// Gets a HierObject's short name
	// ARGS: SELF
	// RETURNS: name <string>
	LFUNC(hier_get_name)
	{
		auto obj = lua::check_object<HierObject>(1);
		lua_pushstring(L, obj->get_name().c_str());
		return 1;
	}

	// Gets a HierObject's parent
	// ARGS: SELF
	// RETURNS: parent <userdata>
	LFUNC(hier_get_parent)
	{
		auto obj = lua::check_object<HierObject>(1);
		auto parent = obj->get_parent();

		if (parent)	lua::push_object(parent);
		else lua_pushnil(L);

		return 1;
	}

	// Adds a Hierarchy child-capable object to a Hierarchy parent-capable object.
	// ARGS: SELF, child<userdata>
	// RETURNS: child<userdata>
	LFUNC(hier_add_child)
	{
		auto parent = check_object<HierObject>(1);
		auto child = check_object<HierObject>(2);

		auto aparent = parent? parent->asp_get<AHierParent>() : nullptr;

		luaL_argcheck(L, aparent != nullptr, 1, "not a hierarchy parent object");

		aparent->add_child(child);

		// return top (child)
		return 1;
	}

	// Gets a Hierarchy child object by name
	// ARGS: SELF, name<string>
	// RETURNS: object<userdata>
	LFUNC(hier_get_child)
	{
		auto parent = check_object<HierObject>(1);
		const char* childname = luaL_checkstring(L, 2);

		auto aparent = parent? parent->asp_get<AHierParent>() : nullptr;

		luaL_argcheck(L, aparent != nullptr, 1, "not a hierarchy parent object");
		
		auto result = aparent->get_child(childname);
		push_object(result);

		// return top (child)
		return 1;
	}

	// Returns all Hierarchy children objects castable to type T
	// ARGS: SELF
	// RETURNS: children<table<name<string>,object<userdata>>>
	template<class T>
	LFUNC(hier_get_children_by_type)
	{
		auto parent = check_object<HierObject>(1);
		auto aparent = parent? parent->asp_get<AHierParent>() : nullptr;

		// Create return table
		lua_newtable(L);

		if (aparent)
		{
			// Get children
			auto children = aparent->get_children_by_type<T>();

			// For each child, create a table entry with the key being the child's name
			for (auto& obj : children)
			{
				const char* objname = obj->get_name().c_str();
				lua::push_object(obj);
				lua_setfield(L, -2, objname);
			}
		}

		// return top (children table)
		return 1;
	}

	// Returns all Hierarchy children objects
	// ARGS: SELF
	// RETURNS: children<table<name<string>,object<userdata>>>
	LFUNC(hier_get_children)
	{
		auto parent = check_object<HierObject>(1);
		auto aparent = parent? parent->asp_get<AHierParent>() : nullptr;

		// Create return table
		lua_newtable(L);

		if (aparent)
		{
			// Get children
			auto children = aparent->get_children();

			// For each child, create a table entry with the key being the child's name
			for (auto& obj : children)
			{
				const char* objname = obj->get_name().c_str();
				lua::push_object(obj);
				lua_setfield(L, -2, objname);
			}
		}

		// return top (children table)
		return 1;
	}

	// Gets the prototype for an AInstantiable, or nil if none exists
	// ARGS: SELF
	// RETURNS: the prototype object <userdata>
	LFUNC(hier_get_prototype)
	{
		HierObject* obj = lua::check_object<HierObject>(1);
		lua::push_object(obj->get_prototype());

		return 1;
	}

	//
	// Network type/direction access (reused for many classes)
	//

	// Gets an object's network type
	// ARGS: SELF
	// RETURNS: net type <string>
	template<class T>
	LFUNC(net_get_type)
	{
		T* obj = lua::check_object<T>(1);
		NetType type = obj->get_type();
		const std::string& type_name = NetTypeDef::get(type)->get_name();

		lua_pushstring(L, type_name.c_str());
		
		return 1;
	}

	// Gets an object's direction
	// ARGS: SELF
	// RETURNS: dir <string>
	template<class T>
	LFUNC(net_get_dir)
	{
		T* obj = lua::check_object<T>(1);
		const char* dir_name = genie::dir_to_str(obj->get_dir());
		
		lua_pushstring(L, Util::str_tolower(dir_name).c_str());
		return 1;
	}

	// Gets a connectable object's currently-bound outgoing or incoming links.
	// When called with no arguments, it gets all links from all endpoints.
	// When called with a nettype argument, it just gets all links of that net type.
	// ARGS: SELF, [netname <string>]
	// RETURNS: links <array<userdata>>
	LFUNC(net_get_links)
	{
		Object* obj = lua::check_object<Object>(1);
		const char* netname = luaL_optstring(L, 2, nullptr);
		NetType nettype = NET_INVALID;

		// Resolve network type, if provided
		if (netname)
		{
			nettype = NetTypeDef::type_from_str(netname);
			luaL_argcheck(L, nettype != NET_INVALID, 2, "unknown network type");
		}
		
		// Create empty result
		lua_newtable(L);
		{
			unsigned int arrindex = 1;

			// Iterate through all endpoint aspects of the object
			auto eps = obj->asp_get_all_matching<AEndpoint>();
			for (AEndpoint* ep : eps)
			{
				// Skip all but the given network type, if provided
				if (nettype != NET_INVALID && ep->get_type() != nettype)
					continue;

				// Otherwise, query the endpoint's links and add them to the result.
				// Multiple calls will update arrindex and concatenate to the same table.
				arrindex = push_array(L, ep->links(), arrindex);
			}
		}

		return 1;
	}


	//
	// System
	//

	// CONSTRUCTOR: Creates a new System with the given name and registers it in GENIE's Hierarchy
	// ARGS: system name <string>
	// RETURNS: the system <userdata>
	LFUNC(system_new)
	{
		const char* sysname = luaL_checkstring(L, 1);

		System* sys = new System(sysname);
		genie::get_root()->add_object(sys);

		lua::push_object(sys);
		return 1;
	}

	// Creates a named Node from a named NodeDef and adds it to the System
	// ARGS: SELF, node/instance name <string>, NodeDef name <string>
	// RETURNS: the new node <userdata>
	LFUNC(system_add_node)
	{
		System* sys = check_object<System>(1);
		std::string instname = luaL_checkstring(L, 2);
		std::string defname = luaL_checkstring(L, 3);
		
		HierObject* def = nullptr;

		try
		{
			def = genie::get_root()->get_object(defname);
		}
		catch (HierNotFoundException&)
		{
			lerror("NodeDef not found: " + defname);
		}

		HierObject* newobj = def->instantiate();


		newobj->set_name(instname);
		sys->add_object(newobj);

		lua::push_object(newobj);

		return 1;
	}

	// Manually creates an Export with a given name, network type, and direction
	// ARGS: SELF, export name <string>, network type <string>, direction(in/out) <string>
	// RETURNS: the new export <userdata>
	LFUNC(system_add_export)
	{
		System* sys = check_object<System>(1);
		std::string exname = luaL_checkstring(L, 2);
		std::string netname = luaL_checkstring(L, 3);
		std::string dirname = luaL_checkstring(L, 4);
		
		NetTypeDef* ndef = NetTypeDef::get(netname);
		luaL_argcheck(L, ndef, 3, "not a valid network type");

		Dir dir = genie::dir_from_str(dirname);
		luaL_argcheck(L, dir != Dir::INVALID, 4, "invalid direction");

		Export* ex = ndef->create_export(dir, exname, sys);
		assert(ex);
		sys->add_object(ex);

		lua::push_object(ex);

		return 1;
	}

	// Creates a new Link between a source and a sink, and optionally
	// of an explicit network type (it's deduced automatically by default).
	// The source and sink can either be Endpoint-equipped HierObjects, or
	// strings representing hierarchy paths.
	// ARGS: SELF (system), source <userdata or string>, 
	//		sink <userdata or string>, [network type <string>]
	// RETURNS: the new Link <userdata>
	LFUNC(system_add_link)
	{
		System* sys = lua::check_object<System>(1);
		auto src = check_obj_or_str_hierpath(L, 2, sys);
		auto sink = check_obj_or_str_hierpath(L, 3, sys);
		auto netstr = luaL_optstring(L, 4, nullptr);

		Link* link;
		if (netstr)
		{
			NetType nettype = NetTypeDef::type_from_str(netstr);
			luaL_argcheck(L, nettype != NET_INVALID, 4, "unknown network type");
			link = sys->connect(src, sink, nettype);
		}
		else
		{
			link = sys->connect(src, sink);
		}

		lua::push_object(link);

		return 1;
	}

	// Gets the System's links. Comes in four varieties:
	// (no args) - returns every link of every type
	// (nettype) - returns every link of the given type
	// (src, sink) - returns all links between src and sink of every nettype
	// (src, sink, nettype) - returns all links between src and sink of given nettype
	// ARGS: SELF, see above. nettype is string. src/sink are userdata or string.
	// RETURNS: links <array<userdata>>
	LFUNC(system_get_links)
	{
		int nargs = lua_gettop(L);
		
		// Get system (self)
		System* sys = lua::check_object<System>(1);

		// Get src/sink, if applicable
		HierObject* src = nargs >= 3 ? check_obj_or_str_hierpath(L, 2, sys) : nullptr;
		HierObject* sink = nargs >= 3 ? check_obj_or_str_hierpath(L, 3, sys) : nullptr;

		// Get and check nettype, if applicable
		NetType nettype = NET_INVALID;
		const char* netstr = nullptr;

		if (nargs == 2) netstr = luaL_checkstring(L, 2);
		else if (nargs == 4) netstr = luaL_checkstring(L, 4);

		if (netstr)
		{
			nettype = NetTypeDef::type_from_str(netstr);
			luaL_argcheck(L, nettype != NET_INVALID, nargs, "unknown network type");
		}

		// Get links
		System::Links links;

		switch (nargs)
		{
		case 1:	links = sys->get_links(); break;
		case 2: links = sys->get_links(nettype); break;
		case 3: links = sys->get_links(src, sink); break;
		case 4: links = sys->get_links(src, sink, nettype); break;
		default: lerror("invalid number of arguments");
		}
		
		// Populate result. It's an array-type table where keys are integers
		lua_newtable(L);
		push_array(L, links);

		return 1;
	}

	LCLASS(System, "System",
	{
		LM(__tostring, hier_get_name),
		LM(get_name, hier_get_name),
		LM(add_node, system_add_node),
		LM(add_export, system_add_export),
		LM(get_object, hier_get_child),
		LM(get_objects, hier_get_children),
		LM(get_nodes, hier_get_children_by_type<Node>),
		LM(get_exports, hier_get_children_by_type<Export>),
		LM(add_link, system_add_link),
		LM(get_links, system_get_links)
	},
	{
		LM(new, system_new)
	});

	//
	// Node
	//

	LCLASS(Node, "Node",
	{
		LM(__tostring, hier_get_name),
		LM(get_name, hier_get_name),
		LM(get_parent, hier_get_parent),
		LM(get_prototype, hier_get_prototype),
		LM(get_ports, hier_get_children_by_type<Port>)
	});

	//
	// NodeDef
	//

	// CONSTRUCTOR
	// Creates a new blank NodeDef and registers it with GENIE
	// ARGS: name <string>
	// RETURNS: nodedef <userdata>
	LFUNC(nodedef_new)
	{
		const char* sysname = luaL_checkstring(L, 1);

		NodeDef* def = new NodeDef(sysname);
		genie::get_root()->add_object(def);

		lua::push_object(def);
		
		return 1;
	}

	// Creates a new PortDef of a given network type and direction and adds it to the NodeDef
	// ARGS: SELF, port name <string>, port nettype <string>, direction <string>
	LFUNC(nodedef_add_portdef)
	{
		NodeDef* ndef = lua::check_object<NodeDef>(1);
		const char* portname = luaL_checkstring(L, 2);
		const char* netname = luaL_checkstring(L, 3);
		const char* dirname = luaL_checkstring(L, 4);

		NetTypeDef* netdef = NetTypeDef::get(netname);
		Dir dir = genie::dir_from_str(dirname);

		luaL_argcheck(L, netdef, 3, "unknown network type");
		luaL_argcheck(L, dir != Dir::INVALID, 4, "unknown direction");

		// Create portdef
		auto portdef = netdef->create_port_def(dir, portname, ndef);

		lua::push_object(portdef);

		return 1;
	}

	LCLASS(NodeDef, "NodeDef",
	{
		LM(__tostring, hier_get_name),
		LM(get_name, hier_get_name),
		LM(add_portdef, nodedef_add_portdef),
		LM(get_portdefs, hier_get_children_by_type<PortDef>)
	},
	{
		LM(new, nodedef_new)
	});

	//
	// Export
	//

	LCLASS(Export, "Export",
	{
		LM(__tostring, hier_get_name),
		LM(get_name, hier_get_name),
		LM(get_parent, hier_get_parent),
		LM(get_type, net_get_type<Export>),
		LM(get_dir, net_get_dir<Export>),
		LM(get_links, net_get_links)
	});

	//
	// Link
	//

	// Gets a link's source object
	// ARGS: SELF
	// RETURNS: object <userdata> or nil
	LFUNC(link_get_src)
	{
		auto link = lua::check_object<Link>(1);
		auto ep = link->get_src();
		if (ep)
			lua::push_object(ep->asp_container());
		else
			lua_pushnil(L);

		return 1;
	}

	// Gets a link's sink object
	// ARGS: SELF
	// RETURNS: object <userdata> or nil
	LFUNC(link_get_sink)
	{
		auto link = lua::check_object<Link>(1);
		auto ep = link->get_sink();
		if (ep)
			lua::push_object(ep->asp_container());
		else
			lua_pushnil(L);

		return 1;
	}

	// Returns a useful string in the form:
	//     source -> sink (nettype)
	// when trying to print a Link object.
	// ARGS: SELF
	// RETURNS: <string>
	LFUNC(link_to_string)
	{
		auto link = lua::check_object<Link>(1);
		std::string srctext = "<unconnected>";
		std::string sinktext = "<unconnected>";

		// Get endpoints and type
		auto src_ep = link->get_src();
		auto sink_ep = link->get_sink();
		NetType type = link->get_type();

		// Get network type string
		const std::string& typetext = NetTypeDef::get(type)->get_name();
		
		// If endpoints are connected, get their full path strings
		if (src_ep) srctext = src_ep->asp_container()->get_full_name();
		if (sink_ep) sinktext = sink_ep->asp_container()->get_full_name();

		// Push result
		lua_pushstring(L, (srctext + " -> " + sinktext + " (" + typetext + ")").c_str());

		return 1;
	}

	LCLASS(Link, "Link",
	{
		LM(__tostring, link_to_string),
		LM(get_type, net_get_type<Link>),
		LM(get_src, link_get_src),
		LM(get_sink, link_get_sink)
	});

	//
	// Port
	//

	LCLASS(Port, "Port",
	{
		LM(__tostring, hier_get_name),
		LM(get_name, hier_get_name),
		LM(get_prototype, hier_get_prototype),
		LM(get_type, net_get_type<Port>),
		LM(get_dir, net_get_dir<Port>),
		LM(get_links, net_get_links)
	});

	//
	// PortDef
	//

	LFUNC(portdef_new)
	{
		return 1;
	}

	LCLASS(PortDef, "PortDef",
	{
		LM(__tostring, hier_get_name),
		LM(get_name, hier_get_name),
		LM(get_parent, hier_get_parent),
		LM(get_prototype, hier_get_prototype),
		LM(get_type, net_get_type<PortDef>),
		LM(get_dir, net_get_dir<PortDef>)
	},
	{
		LM(new, portdef_new)
	});
}

