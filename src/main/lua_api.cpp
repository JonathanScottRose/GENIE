#include "genie/genie.h"
#include "genie/structure.h"
#include "genie/hierarchy.h"
#include "genie/connections.h"
#include "genie/node_split.h"
#include "genie/node_merge.h"
#include "genie/node_reg.h"
#include "genie/vlog.h"
#include "genie/vlog_bind.h"
#include "genie/lua/genie_lua.h"
#include "genie/net_rs.h"
#include "flow.h"

using namespace genie;
using namespace lua;


// LDoc package description

/// Global functions and classes provided natively by the GENIE executable. 
/// The contents of this package come pre-loaded into the global environment in a table called 'genie'.
/// @module genie

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
			result = parent->get_child_as<T>(path);
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

	// Extracts a list of Objects from a Lua array/set
	// Array: objects are table values
	// Set: objects are table keys
	// ARGS: table
	// RETURNS: list of objects
	template<class T>
	List<T*> get_array_or_set(lua_State* L, int tablepos)
	{
		List<T*> result;
		tablepos = lua_absindex(L, tablepos);

		lua_pushnil(L);
		while (lua_next(L, tablepos))
		{
			// Try key
			T* obj = lua::is_object<T>(-2);
			
			// Try value
			if (!obj)
				obj = lua::is_object<T>(-1);

			if (!obj)
			{
				lua::lerror("table entry doesn't have " + lua::obj_typename<T>() + 
				" as either key or value");
			}

			result.push_back(obj);
			lua_pop(L, 1);
		}

		return result;
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
		const std::string& type_name = Network::get(type)->get_name();

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
		
		lua_pushstring(L, util::str_tolower(dir_name).c_str());
		return 1;
	}

	// Gets a Port's currently-bound outgoing or incoming links.
	// When called with no arguments, it gets all connections of the Port's canonical network type
	// When called with a nettype argument, it just gets all links of that net type.
	// ARGS: SELF, [netname <string>]
	// RETURNS: links <array<userdata>>
	LFUNC(net_get_links)
	{
		Port* obj = lua::check_object<Port>(1);
		const char* netname = luaL_optstring(L, 2, nullptr);
		NetType nettype = obj->get_type();

		// Resolve network type, if provided
		if (netname)
		{
			nettype = Network::type_from_str(netname);
			luaL_argcheck(L, nettype != NET_INVALID, 2, "unknown network type");
		}
		
		// Create empty result
		lua_newtable(L);
		{
			unsigned int arrindex = 1;

			// Iterate through all endpoint aspects of the object
			auto ep = obj->get_endpoint_sysface(nettype);
			push_array(L, ep->links());
		}

		return 1;
	}
	
	//
	// Globals
	//
	
	// Forward decls
	template<class T>
	LFUNC(hier_get_children_by_type);
	LFUNC(hier_get_children);
	LFUNC(hier_get_child);
	
	/// Get a reference to a GENIE object by hierarchical name.
	/// @function get_object
	/// @tparam string name absolute path of object
	/// @treturn string reference to the object
	/// @raise error if object not found
	LFUNC(glob_get_object)
	{
		lua::push_object(genie::get_root());
		lua_insert(L, 1);
		return hier_get_child(L);
	}

	/// Get all immediate children of the root (all @{System} and @{Node} prototypes)
	/// @function get_objects
	/// @treturn table(string,HierObject) child objects keyed by name
	LFUNC(glob_get_objects)
	{
		lua::push_object(genie::get_root());
		lua_insert(L, 1);
		return hier_get_children(L);
	}

	/// Get all @{System}s.
	/// @function get_systems
	/// @treturn table(string,System) Systems keyed by name
	LFUNC(glob_get_systems)
	{
		lua::push_object(genie::get_root());
		lua_insert(L, 1);
		return hier_get_children_by_type<System>(L);
	}

	/// Marks a set of RS Links as temporally exclusive
	/// @function make_exclusive
	/// @tparam array(Link)|Set(Link) links Array or Set of RS Links
	LFUNC(glob_make_exclusive)
	{
		auto links = get_array_or_set<RSLink>(L, 1);
		ARSExclusionGroup::process_and_create(links);
		return 0;
	}

	/// Ceiling of log base 2
	/// @function clog2
	/// @tparam number x Unsigned integer
	/// @treturn number
	LFUNC(glob_clog2)
	{
		auto num = luaL_checkunsigned(L, 1);
		auto result = util::log2(num);
		lua_pushunsigned(L, result);
		return 1;
	}

	LGLOBALS(
	{
		LM(get_object, glob_get_object),
		LM(get_objects, glob_get_objects),
		LM(get_systems, glob_get_systems),
		LM(make_exclusive, glob_make_exclusive),
		LM(clog2, glob_clog2)
	});

	//
	// HierObject (re-used for many classes)
	//

	/// Base class for objects in the GENIE design hierarchy.
	///
	/// Direct superclass of: @{Node}, @{Port}, @{RSLinkpoint}.
	///
	/// Indirect superclass of: @{System}, @{RSPort}
	/// @type HierObject
	
	/// Get the object's name
	/// @function get_name
	/// @treturn string name
	LFUNC(hier_get_name)
	{
		auto obj = lua::check_object<HierObject>(1);
		lua_pushstring(L, obj->get_name().c_str());
		return 1;
	}

	/// Get the object's absolute path.
	/// @function get_hier_path
	/// @treturn string path
	LFUNC(hier_get_path)
	{
		auto obj = lua::check_object<HierObject>(1);
        
        HierObject* rel_obj = nullptr;
        if (!lua_isnil(L, 2))
            rel_obj = lua::check_object<HierObject>(2);

		lua_pushstring(L, obj->get_hier_path(rel_obj).c_str());
		return 1;
	}

	/// Get the object's parent.
	/// @function get_parent
	/// @treturn HierObject handle to parent object or nil if called on the hierarchy root
	LFUNC(hier_get_parent)
	{
		auto obj = lua::check_object<HierObject>(1);
		auto parent = obj->get_parent();

		if (parent)	lua::push_object(parent);
		else lua_pushnil(L);

		return 1;
	}

	// Adds a child object (internal)
	// ARGS: self, child<userdata>
	// RETURNS: child<userdata>
	LFUNC(hier_add_child)
	{
		auto parent = check_object<HierObject>(1);
		auto child = check_object<HierObject>(2);

		parent->add_child(child);

		// return top (child)
		return 1;
	}

	/// Get a child object by name.
	/// @function get_child
	/// @tparam string name hierarchical path to child object, relative to this object
	/// @treturn HierObject handle of the child object
	/// @raise error if object does not exist
	LFUNC(hier_get_child)
	{
		auto parent = check_object<HierObject>(1);
		const char* childname = luaL_checkstring(L, 2);
		
		auto result = parent->get_child(childname);
		push_object(result);

		// return top (child)
		return 1;
	}

	// Returns all Hierarchy children objects castable to type T (internal)
	// ARGS: SELF
	// RETURNS: children<table<name<string>,object<userdata>>>
	template<class T>
	LFUNC(hier_get_children_by_type)
	{
		auto parent = check_object<HierObject>(1);

		// Create return table
		lua_newtable(L);

		if (parent)
		{
			// Get children
			auto children = parent->get_children_by_type<T>();

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

	/// Returns all immediate children
	/// @function get_children
	/// @treturn table(string,HierObject) child objects keyed by name
	LFUNC(hier_get_children)
	{
		auto parent = check_object<HierObject>(1);

		// Create return table
		lua_newtable(L);

		if (parent)
		{
			// Get children
			auto children = parent->get_children();

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

	/// Lua metafunction for converting to @{string}.
	/// 
	/// Calls @{HierObject:get_hier_path}.
	/// @see HierObject:get_hier_path
	/// @function __tostring
	/// @treturn string
	LCLASS(HierObject, 
	{
		LM(__tostring, hier_get_path),
		LM(get_name, hier_get_name),
		LM(get_hier_path, hier_get_path),
		LM(get_parent, hier_get_parent),
        LM(get_child, hier_get_child),
		LM(get_children, hier_get_children)
	});

	//
	// Node
	//

	/// Represents a Verilog module or its instance. 
	///
	/// Inherits from: @{HierObject}.
	/// Direct superclass of: @{System}.
	/// @type Node
	
	/// Constructor.
	///
	/// Creates a blank Node and registers it with GENIE. 
	/// It's static and called with genie.Node.new(...)
	/// @function new
	/// @tparam string name @{Node}'s name
	/// @tparam string modname Verilog module name
	/// @treturn Node new instance
	LFUNC(node_new)
	{
		const char* name = luaL_checkstring(L, 1);
		const char* mod_name = luaL_checkstring(L, 2);

		// Create new verilog module definition
		auto vinfo = new vlog::NodeVlogInfo(mod_name);

		// Create and register the node
		Node* node = new Node();
		node->set_name(name);
		node->set_hdl_info(vinfo);
		genie::get_root()->add_child(node);

		lua::push_object(node);
		
		return 1;
	}

	/// Creates a new Port within the Node.
	/// @function add_port
	/// @tparam string name port name
	/// @tparam string type network type
	/// @tparam string dir one of `in`, `out`, `bidir`
	/// @treturn Port
	LFUNC(node_add_port)
	{
		Node* node = lua::check_object<Node>(1);
		const char* portname = luaL_checkstring(L, 2);
		const char* netname = luaL_checkstring(L, 3);
		const char* dirname = luaL_checkstring(L, 4);

		Network* netdef = Network::get(netname);
		Dir dir = genie::dir_from_str(dirname);

		luaL_argcheck(L, netdef, 3, "unknown network type");
		luaL_argcheck(L, dir != Dir::INVALID, 4, "unknown direction");

		// Create port
		auto port = netdef->create_port(dir);
		port->set_name(portname);
		node->add_child(port);

		lua::push_object(port);

		return 1;
	}

	/// Defines and/or sets a parameter.
	///
	/// A parameter can be defined without providing a value. One can be provided later
	/// with a second call.
	/// @function def_param
	/// @tparam string parameter name
	/// @tparam[opt] string|number value value
	/// @raise error if parameter already defined, or bad value expression
	LFUNC(node_def_param)
	{
		auto self = lua::check_object<Node>(1);
		std::string parmname = luaL_checkstring(L, 2);
		
		// case-insensitive
		util::str_makeupper(parmname);

		bool exists = self->has_param(parmname);
		bool provide_val = !lua_isnoneornil(L, 3);

		ParamBinding* parm = exists? self->get_param(parmname) : 
			self->define_param(parmname);
		
		if (exists && !provide_val)
		{
			lerror("parameter " + parmname + " already defined");
		}
		else if (provide_val)
		{
			std::string parmval = luaL_checkstring(L, 3);
			parm->set_expr(parmval);
		}
		
		return 0;
	};

	/// Get all @{Port}s.
	/// @function get_ports
	/// @treturn array(Port)
	
	/// Get @{Port} by name.
	/// @function get_port
	/// @tparam string name name of @{Port}
	/// @treturn Port
	LSUBCLASS(Node, (HierObject),
	{
		LM(add_port, node_add_port),
		LM(get_ports, hier_get_children_by_type<Port>),
		LM(get_port, hier_get_child),
		LM(def_param, node_def_param)
	},
	{
		LM(new, node_new)
	});

	//
	// System
	//

	/// A @{Node} prototype that can contain @{Node} instances.
	///
	/// Inherits from: @{Node}, @{HierObject}.
	/// @type System
	
	// CONSTRUCTOR: Creates a new System with the given name and registers it in GENIE's Hierarchy
	// ARGS: system name <string>, topology function <functoin>
	// RETURNS: the system <userdata>
	
	/// Constructor.
	///
	/// Creates a new System and registers it with GENIE. Call using genie.System.new(...)
	/// @function new
	/// @tparam string name System name
	/// @treturn System
	LFUNC(system_new)
	{
		const char* sysname = luaL_checkstring(L, 1);
		luaL_checktype(L, 2, LUA_TFUNCTION);

		// Create new verilog module definition
		auto vinfo = new vlog::SystemVlogInfo(sysname);

		// Create the System
		System* sys = new System();
		sys->set_name(sysname);
		sys->set_hdl_info(vinfo);
		genie::get_root()->add_child(sys);

		// Makes a ref for topo function, attach to system
		auto atopo = new ATopoFunc();
		atopo->func_ref = lua::make_ref();
		sys->asp_add(atopo);		

		lua::push_object(sys);
		return 1;
	}

	/// Instantiates a @{Node} within this System.
	/// @function add_node
	/// @tparam string name instance name
	/// @tparam string|Node prototype absolute hierarchy path or handle of Node to instantiate
	/// @treturn Node the new instance
	LFUNC(system_add_node)
	{
		System* sys = check_object<System>(1);
		std::string instname = luaL_checkstring(L, 2);
		auto prototype = check_obj_or_str_hierpath<Node>(L, 3);

		luaL_argcheck(L, prototype, 3, "node not found");

		HierObject* newobj = prototype->instantiate();

		newobj->set_name(instname);
		sys->add_child(newobj);

		lua::push_object(newobj);

		return 1;
	}

	/// Create a Split Node.
	///
	/// Should only be called within Topology Functions.
	/// @function add_split
	/// @tparam string name name
	/// @treturn Node
	LFUNC(system_add_split)
	{
		System* self = check_object<System>(1);
		const char* name = luaL_checkstring(L, 2);

		auto node = new NodeSplit();
		node->set_name(name);
		self->add_child(node);

		lua::push_object(node);

		return 1;
	}

	/// Create a Merge Node.
	///
	/// Should only be called within Topology Functions.
	/// @function add_merge
	/// @tparam string name name
	/// @treturn Node
	LFUNC(system_add_merge)
	{
		System* self = check_object<System>(1);
		const char* name = luaL_checkstring(L, 2);

		auto node = new NodeMerge();
		node->set_name(name);
		self->add_child(node);

		lua::push_object(node);

		return 1;
	}

	/// Create a Buffer Node.
	///
	/// Should only be called within Topology Functions.
	/// @function add_buf
	/// @tparam string name name
	/// @treturn Node
	LFUNC(system_add_buf)
	{
		System* self = check_object<System>(1);
		const char* name = luaL_checkstring(L, 2);

		auto node = new NodeReg();
		node->set_name(name);
		self->add_child(node);

		lua::push_object(node);

		return 1;
	}

	/// Splices a @{Node} instance into the middle of an existing Link.
	///
	/// The @{Node} must already be an instance within a @{System}.
	/// The Node must have a single input and single output @{Port} of the same network type
	/// as the original @{Link}. Post-splice, the original @{Link} will terminate at the input 
	/// @{Port} of the provided object, and a new @{Link} will be created from its output @{Port}
	/// and terminate wherever the original @{Link} did.
	/// @function splice_node
	/// @tparam Link link the original Link to splice
	/// @tparam string|Node obj absolute hierarchy path or handle to the @{Node} instance
	/// @treturn Link the new second Link
	LFUNC(system_splice_node)
	{
		System* self = check_object<System>(1);
		Link* link = check_object<Link>(2);
		HierObject* obj = check_obj_or_str_hierpath(L, 3, self);

		auto new_link = self->splice(link, obj, obj);

		lua::push_object(new_link);
		return 1;
	}
	
	/// Creates a new @{Link}.
	///
	/// Creates a new @{Link} between a source and a sink. The network type
	/// is automatically deduced by default, but can be explicitly specified too.
	/// @function add_link
	/// @tparam string|HierObject source hierarchy path or reference to source object
	/// @tparam string|HierObject sink hierarchy path or reference to sink object
	/// @tparam[opt] string type network type
	/// @treturn Link
	LFUNC(system_add_link)
	{
		System* sys = lua::check_object<System>(1);
		auto src = check_obj_or_str_hierpath<HierObject>(L, 2, sys);
		auto sink = check_obj_or_str_hierpath<HierObject>(L, 3, sys);
		auto netstr = luaL_optstring(L, 4, nullptr);

		Link* link;
		if (netstr)
		{
			NetType nettype = Network::type_from_str(netstr);
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
	
	/// Gets the @{System}'s Links.
	///
	/// Comes in four varieties:  
	/// (no args) - returns every link of every type  
	/// (nettype) - returns every link of the given type  
	/// (src, sink) - returns all links between src and sink of every nettype  
	/// (src, sink, nettype) - returns all links between src and sink of given nettype  
	/// @function get_links
	/// @tparam[opt] string|HierObject src source
	/// @tparam[opt] string|HierObject sink sink
	/// @tparam[opt] string type network type
	/// @treturn table array of @{Link}
	LFUNC(system_get_links)
	{
		int nargs = lua_gettop(L);
		
		// Get system (self)
		System* sys = lua::check_object<System>(1);

		// Get src/sink, if applicable
		HierObject* src = nargs >= 3 ? check_obj_or_str_hierpath<HierObject>(L, 2, sys) : nullptr;
		HierObject* sink = nargs >= 3 ? check_obj_or_str_hierpath<HierObject>(L, 3, sys) : nullptr;

		// Get and check nettype, if applicable
		NetType nettype = NET_INVALID;
		const char* netstr = nullptr;

		if (nargs == 2) netstr = luaL_checkstring(L, 2);
		else if (nargs == 4) netstr = luaL_checkstring(L, 4);

		if (netstr)
		{
			nettype = Network::type_from_str(netstr);
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
	
	/// Exports a @{Port} of a @{Node} instance within the @{System}.
	///
	/// Automatically creates a @{Link} between the original and exported @{Port}.
	/// @function make_export
	/// @tparam string|Port port name or handle of existing @{Port} to export
	/// @tparam string name name for exported @{Port}
	/// @treturn @{Port} exported @{Port}
	LFUNC(system_make_export)
	{
		auto self = lua::check_object<System>(1);
		auto port = check_obj_or_str_hierpath<Port>(L, 2, self);
		const char* exname = luaL_checkstring(L, 3);

		// Get network def for port type
		auto ndef = Network::get(port->get_type());

		// Call the thing
		auto result = ndef->export_port(self, port, exname);
		lua::push_object(result);

		return 1;
	}

	/// Creates an RS latency query.
	///
	/// Creates a new @{System} parameter containing the latency of the specified RS @{Link}.
	/// @function create_latency_query
	/// @tparam Link link the RS @{Link} to query
	/// @tparam string parmname the name of the parameter to create
	LFUNC(system_create_latency_query)
	{
		auto self = lua::check_object<System>(1);
		auto link = lua::check_object<RSLink>(2);
		auto paramname = luaL_checkstring(L, 3);

		// Queries are attached to the System
		auto asp = self->asp_get<ARSLatencyQueries>();
		if (!asp)
			asp = self->asp_add(new ARSLatencyQueries());

		asp->add(link, paramname);

		return 0;
	}

    /// Returns un-topologized RS Links.
    ///
    /// Returns all RS links whose source and sink Ports lack
    /// Topology connections.
    ///
    /// Useful for writing topology functions.
    /// @function get_untopo_rs_links
    /// @treturn array(Link)
    LFUNC(system_get_untopo_rs_links)
    {
        auto self = lua::check_object<System>(1);

        // Get all RS links and then filter them out.
        auto all_links = self->get_links(NET_RS);
        List<Link*> result;

        for (auto link : all_links)
        {
            auto rslink = (RSLink*)link;

            RSPort* src_rs = RSPort::get_rs_port(rslink->get_src());
            RSPort* sink_rs = RSPort::get_rs_port(rslink->get_sink());
            TopoPort* src_topo = src_rs->get_topo_port();
            TopoPort* sink_topo = sink_rs->get_topo_port();

            // Only those links where src and sink topo ports are unconnected
            if (!src_topo->is_connected(NET_TOPO) && !sink_topo->is_connected(NET_TOPO))
                result.push_back(link);
        }

        lua_newtable(L);
        push_array(L, result);

        return 1;
    }

	/// Get all contained @{Node}s.
	/// @function get_nodes
	/// @treturn array(Node)
	
	/// Alias for @{HierObject:get_child}.
	/// @tparam string name
	/// @see HierObject:get_child
	/// @function get_object
	/// @treturn HierObject
	
	/// Alias for @{HierObject:get_children}.
	/// @see HierObject:get_children
	/// @function get_objects
	/// @treturn array(HierObject)
	
	/// Get all @{Port}s.
	///
	/// These are the @{Port}s that connect the @{System} to the outside. This is consistent with a @{System} actually
	/// being a @{Node} that can be instantiated elsewhere.
	/// @function get_ports
	/// @treturn array(Port)
	
	LSUBCLASS(System, (Node),
	{
		
		LM(add_node, system_add_node),
		LM(get_nodes, hier_get_children_by_type<Node>),
		LM(get_ports, hier_get_children_by_type<Port>),
		LM(add_link, system_add_link),
		LM(get_links, system_get_links),
		LM(add_split, system_add_split),
		LM(add_buffer, system_add_buf),
		LM(splice_node, system_splice_node),
		LM(add_merge, system_add_merge),
		LM(make_export, system_make_export),
		LM(create_latency_query, system_create_latency_query),
        LM(get_object, hier_get_child),
        LM(get_objects, hier_get_children),
        LM(get_untopo_rs_links, system_get_untopo_rs_links)
	},
	{
		LM(new, system_new)
	});

	//
	// Link
	//

	/// @type Link
	
	/// Get source object
	/// @function get_src
	/// @treturn Port|nil
	LFUNC(link_get_src)
	{
		auto link = lua::check_object<Link>(1);
		lua::push_object(link->get_src());

		return 1;
	}

	/// Get sink object
	/// @function get_sink
	/// @treturn Port|nil
	LFUNC(link_get_sink)
	{
		auto link = lua::check_object<Link>(1);
		lua::push_object(link->get_sink());

		return 1;
	}

	/// Lua metafunction for converting to @{string}.
	///
	/// Returns a useful string in the form:
	///     source -> sink (nettype)
	/// @function __tostring
	/// @treturn string
	LFUNC(link_to_string)
	{
		auto link = lua::check_object<Link>(1);
		std::string srctext = "<unconnected>";
		std::string sinktext = "<unconnected>";

		// Get endpoints and type
		auto src = link->get_src();
		auto sink = link->get_sink();
		NetType type = link->get_type();

		// Get network type string
		const std::string& typetext = Network::get(type)->get_name();
		
		// If endpoints are connected, get their full path strings
		if (src) srctext = src->get_hier_path();
		if (sink) sinktext = sink->get_hier_path();

		// Push result
		lua_pushstring(L, (srctext + " -> " + sinktext + " (" + typetext + ")").c_str());

		return 1;
	}

	// Adds a direct parent link
	// ARGS: SELF, other link <userdata>
	// RETURNS: SELF
	
	/// Add parent @{Link}.
	///
	/// The other @{Link} will become one of this @{Link}'s parents.
	/// This @{Link} will automatically be added as one of the other @{Link}'s children.
	/// @function add_parent
	/// @tparam Link link parent @{Link}
	/// @treturn Link parent @{Link}
	LFUNC(link_add_parent)
	{
		Link* self = lua::check_object<Link>(1);
		Link* other = lua::check_object<Link>(2);

		auto cont = self->asp_get<ALinkContainment>();
		if (cont)
		{
			cont->add_parent_link(other);
		}

		lua_pushvalue(L, 1);
		return 1;
	}

	/// Add child @{Link}.
	///
	/// The other @{Link} will become one of this @{Link}'s children.
	/// This @{Link} will automatically be added as one of the other @{Link}'s parents.
	/// @function add_child
	/// @tparam Link link child @{Link}
	/// @treturn Link child @{Link}
	LFUNC(link_add_child)
	{
		Link* self = lua::check_object<Link>(1);
		Link* other = lua::check_object<Link>(2);

		auto cont = self->asp_get<ALinkContainment>();
		if (cont)
		{
			cont->add_child_link(other);
		}

		lua_pushvalue(L, 1);
		return 1;
	}

	/// Get all direct and indirect child @{Link}s.
	///
	/// Considers children and children's children and so on, finding all Links of a given network type.
	/// @function get_children
	/// @tparam string type network type
	/// @treturn array(Link)
	LFUNC(link_get_children)
	{
		Link* self = lua::check_object<Link>(1);
		const char* netname = luaL_checkstring(L, 2);

		NetType nettype = Network::type_from_str(netname);
		luaL_argcheck(L, nettype != NET_INVALID, 2, "unknown network type");

		lua_newtable(L);

		auto cont = self->asp_get<ALinkContainment>();
		if (cont)
		{
			auto result = cont->get_all_child_links(nettype);
			push_array(L, result);
		}

		return 1;
	}

	/// Get all direct and indirect parent @{Link}s.
	///
	/// Considers parents and parents' parents and so on, finding all Links of a given network type.
	/// @function get_parents
	/// @tparam string type network type
	/// @treturn array(Link)
	LFUNC(link_get_parents)
	{
		Link* self = lua::check_object<Link>(1);
		const char* netname = luaL_checkstring(L, 2);

		NetType nettype = Network::type_from_str(netname);
		luaL_argcheck(L, nettype != NET_INVALID, 2, "unknown network type");

		lua_newtable(L);

		auto cont = self->asp_get<ALinkContainment>();
		if (cont)
		{
			auto result = cont->get_all_parent_links(nettype);
			push_array(L, result);
		}

		return 1;
	}
	
	LCLASS(Link,
	{
		LM(__tostring, link_to_string),
		LM(get_type, net_get_type<Link>),
		LM(get_src, link_get_src),
		LM(get_sink, link_get_sink),
		LM(add_parent, link_add_parent),
		LM(add_child, link_add_child),
		LM(get_all_parents, link_get_parents),
		LM(get_all_children, link_get_children)
	});

	//
	// Port
	//

	/// Endpoint for communication. Is owned by a @{Node}. Associates a subset of the @{Node}'s Verilog module's ports
	/// with a single communication-related role (clock sink, Routed Streaming source, etc).
	///
	/// Inherits from: @{HierObject}.
	///
	/// Direct superclass of: @{RSPort}.
	/// @type Port
	
	/// Associate a Verilog input/output port with the @{Port}.
	///
	/// The allowable roles depend on this @{Port}'s type. The direction of the signal is implied by the @{Port}'s 
	/// direction and the signal's role. For some roles, many signals can be added to a @{Port} with the same role, and
	/// require further differentiation via a string tag parameter. The signal width may contain an expression and
	/// reference parameters.
	/// @function add_signal
	/// @tparam string role the signal's role
	/// @tparam[opt] string tag a user-defined tag unique among this @{Port}'s other signals of the same role
	/// @tparam string name the name of the input/output port in the Verilog module
	/// @tparam string|number width signal width in bits
	LFUNC(port_add_signal)
	{
		// Two function signatures: one with tag, one without
		int nargs = lua_gettop(L);
		bool has_tag;

		if (nargs == 5) has_tag = true;
		else if (nargs == 4) has_tag = false;
		else lerror("invalid number of arguments");

		Port* self = lua::check_object<Port>(1);
		std::string sigrole_str = luaL_checkstring(L, 2);
		std::string tag = has_tag? luaL_checkstring(L, 3) : "";
		std::string vlog_portname = luaL_checkstring(L, has_tag? 4 : 3);

		// Total width of signal, width of signal actually used to bind role, and lsb
		std::string widthexpr_total;
		std::string widthexpr_bind;
		std::string lsbexpr;

		// The argument can either be an array (=table) containing {totalwidth,[boundwidth],[lsb]}
		// or a string just containing totalwidth.
		int tab_idx = has_tag? 5 : 4;
		if (lua_istable(L, tab_idx))
		{
			lua_rawgeti(L, tab_idx, 1);
			lua_rawgeti(L, tab_idx, 2);
			lua_rawgeti(L, tab_idx, 3);

			if (!lua_isstring(L, -3))
				luaL_argerror(L, tab_idx, "expected totalwidth expression as first table element");
			widthexpr_total = lua_tostring(L, -3);

			// Second element (bound width) is optional, defaults to total width
			if (lua_isstring(L, -2))
				widthexpr_bind = lua_tostring(L, -2);
			else if (lua_isnil(L, -2))
				widthexpr_bind = widthexpr_total;
			else
				luaL_argerror(L, tab_idx, "expected bound-width expression as second table element");

			// Third element (lsb) is optional, defaults to 0
			if (lua_isstring(L, -1))
				lsbexpr = lua_tostring(L, -1);
			else if (lua_isnil(L, -1))
				lsbexpr = "0";
			else
				luaL_argerror(L, tab_idx, "expected lsb expression as third table element");

			// Pop the elements retrieved by rawgeti's
			lua_pop(L, 3);
		}
		else if (lua_isstring(L, tab_idx))
		{
			widthexpr_total = lua_tostring(L, tab_idx);
			widthexpr_bind = widthexpr_total;
			lsbexpr = "0";
		}

		// Get Role ID and Role Definition
		Network* netdef = Network::get(self->get_type());
		
		SigRoleID sigrole_id = netdef->role_id_from_name(sigrole_str);
		if (sigrole_id == ROLE_INVALID)
		{
			throw HierException(self, "unknown signal role " + sigrole_str + " for port of type " +
			netdef->get_name());
		}

		auto sigrole = SigRole::get(sigrole_id);

		// Access Port's Node's vlog module and create a new vlog port if it does not exist yet
		Node* node = self->get_node();
		auto vinfo = as_a<vlog::NodeVlogInfo*>(node->get_hdl_info());
		vlog::Port* vport = vinfo->get_port(vlog_portname);

		if (!vport)
		{
			auto vpdir = vlog::Port::make_dir(self->get_dir(), sigrole->get_sense());
			vport = new vlog::Port(vlog_portname, widthexpr_total, vpdir);
			vinfo->add_port(vport);
		}

		// Add a new signal role binding, bound to the entire verilog port
		self->add_role_binding(sigrole_id, tag, new vlog::VlogStaticBinding(vlog_portname, widthexpr_bind, lsbexpr));

		return 0;
	}

	/// Get network type.
	/// @function get_type
	/// @treturn string network type
	
	/// Get direction.
	/// @function get_dir
	/// @treturn string port direction
	
	/// Get connected @{Link}s.
	///
	/// Optionally filters by network type.
	/// @function get_links
	/// @tparam[opt] string type network type
	/// @treturn array(Link)
	
	LSUBCLASS(Port, (HierObject),
	{
		LM(get_type, net_get_type<Port>),
		LM(get_dir, net_get_dir<Port>),
		LM(get_links, net_get_links),
		LM(add_signal, port_add_signal)
	});

	//
	// RSPort
	//

	/// A Routed Streaming Port.
	///
	/// Can have Linkpoints. Created and returned by @{Node:add_port} when given a network type of `rs`.
	///
	/// Subclass of @{Port}.
	/// @type RSPort
	
	// Creates and adds a new linkpoint to a RS portdef
	// ARGS: SELF, lp name <string>, lp encoding <string>, lp type <string>
	
	/// Defines a new Linkpoint.
	///
	/// Only `broadcast` linkpoints are currently supported.
	/// @function add_linkpoint
	/// @tparam string name Linkpoint's name, unique within this @{RSPort}
	/// @tparam string id Linkpoint ID, specified in Verilog constant notation (eg. 3'b101)
	/// @tparam string type must be `broadcast` for now
	/// @treturn RSLinkpoint
	LFUNC(rsport_add_linkpoint)
	{
		RSPort* self = lua::check_object<RSPort>(1);
		const char* lpname = luaL_checkstring(L, 2);
		const char* encstr = luaL_checkstring(L, 3);
		const char* typestr = luaL_checkstring(L, 4);

		RSLinkpoint::Type type = RSLinkpoint::type_from_str(typestr);
		luaL_argcheck(L, type != RSLinkpoint::INVALID, 4, "bad linkpoint type");

		auto lpdef = new RSLinkpoint(self->get_dir(), type);
		lpdef->set_name(lpname);
		lpdef->set_encoding(std::string(encstr));
		self->add_child(lpdef);

		lua::push_object(lpdef);		

		return 1;
	};

	// Gets this RS port's associated TOPO port
	// ARGS: SELF
	// RETURNS: Topo port
	/// Get associated Topology @{Port}.
	///
	/// @{RSPort}s are used to define end-to-end Routed Streaming connections that travel over a physical network
	/// with some topology. Topology Ports and Topology Links define this physical connectivity. This function obtains
	/// this @{RSPort}'s associated Topology Port to allow the writing of Topology Functions that create the networks
	/// that carry RS Links. As such, this function is only usable from within Topology Functions.
	/// @function get_topo_port
	/// @treturn Port
	LFUNC(rsport_get_topo_port)
	{
		// Use -1 to mean 'top of stack' because we may be
		// called directly (through C) from rslp_get_topo_port
		auto self = lua::check_object<RSPort>(-1);

		lua::push_object(self->get_topo_port());
		return 1;
	}

	/// Returns itself.
	///
	/// RS @{Link}s may terminate at either @{RSPort}s or @{RSLinkpoint}s. It is useful to be able to obtain a handle
	/// to the physical @{RSPort} that an RS Link is connected to, by calling this function.
	/// @function get_rs_port
	/// @treturn RSPort self
	LFUNC(rsport_get_rs_port)
	{
		auto self = lua::check_object<RSPort>(1);
		(void)self;
		return 1;
	}

	// Sets this RS port's associated clock port name
	// ARGS: SELF, portname <string>
	// RETURNS: nil
	/// Sets associated clock @{Port} by name.
	///
	/// All @{RSPort}s must be synchronous to a known clock, and this is done by associating an @{RSPort} with
	/// a clock source or sink @{Port} belonging to the same @{Node}.
	/// @function set_clock_port_name
	/// @tparam string portname name of associated clock @{Port}
	LFUNC(rsport_set_clock_port_name)
	{
		auto self = lua::check_object<RSPort>(1);
		const char* clkportnm = luaL_checkstring(L, 2);

		self->set_clock_port_name(clkportnm);

		return 0;
	}

	LSUBCLASS(RSPort, (Port),
	{
		LM(add_linkpoint, rsport_add_linkpoint),
		LM(get_rs_port, rsport_get_rs_port),
		LM(get_topo_port, rsport_get_topo_port),
		LM(set_clock_port_name, rsport_set_clock_port_name)
	});

	//
	// RSLinkpoint
	//

	/// A named sub-entity residing within an @{RSPort}. Can be a source or sink for RS @{Link}s.
	///
	/// Inherits from: @{HierObject}.
	/// @type RSLinkpoint
	
	/// Get parent @{RSPort}'s associated Topology @{Port}.
	/// @function get_topo_port
	/// @treturn Port
	LFUNC(rslp_get_topo_port)
	{
		auto self = lua::check_object<RSLinkpoint>(1);
		lua::push_object(RSPort::get_rs_port(self)->get_topo_port());
		return 1;
	}

	/// Get parent @{RSPort}.
	///
	/// RS @{Link}s may terminate at either @{RSPort}s or @{RSLinkpoint}s. It is useful to be able to obtain a handle
	/// to the physical @{RSPort} that an RS Link is connected to, by calling this function.
	/// @function get_rs_port
	/// @treturn RSPort parent
	LFUNC(rslp_get_rs_port)
	{
		auto self = lua::check_object<RSLinkpoint>(1);
		lua::push_object(self->get_parent());
		return 1;
	}

	/// Get connected @{Link}s.
	/// @function get_links
	/// @tparam[opt] string type network type
	/// @treturn array(Link)
	LSUBCLASS(RSLinkpoint, (Port),
	{
		//LM(get_links, net_get_links),
		LM(get_rs_port, rslp_get_rs_port),
		LM(get_topo_port, rslp_get_topo_port)
	});
}

