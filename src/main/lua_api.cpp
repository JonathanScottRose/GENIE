#include "genie/genie.h"
#include "genie/structure.h"
#include "genie/hierarchy.h"
#include "genie/connections.h"
#include "genie/node_split.h"
#include "genie/node_merge.h"
#include "genie/vlog.h"
#include "genie/vlog_bind.h"
#include "genie/lua/genie_lua.h"
#include "net_rs.h"
#include "flow.h"

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

	// Gets a HierObject's full name
	// ARGS: SELF
	// RETURNS: name <string>
	LFUNC(hier_get_path)
	{
		auto obj = lua::check_object<HierObject>(1);
		lua_pushstring(L, obj->get_hier_path().c_str());
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

		parent->add_child(child);

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
		
		auto result = parent->get_child(childname);
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

	// Returns all Hierarchy children objects
	// ARGS: SELF
	// RETURNS: children<table<name<string>,object<userdata>>>
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

	LCLASS(HierObject, "HierObject",
	{
		LM(__tostring, hier_get_path),
		LM(get_name, hier_get_name),
		LM(get_hier_path, hier_get_path),
		LM(get_parent, hier_get_parent),
		LM(get_children, hier_get_children)
	});

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
			nettype = Network::type_from_str(netname);
			luaL_argcheck(L, nettype != NET_INVALID, 2, "unknown network type");
		}
		
		// Create empty result
		lua_newtable(L);
		{
			unsigned int arrindex = 1;

			// Iterate through all endpoint aspects of the object
			auto eps = obj->asp_get_all_matching<Endpoint>();
			for (Endpoint* ep : eps)
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
	// Globals
	//

	// Gets a hierarchy object from the root

	LFUNC(glob_get_object)
	{
		lua::push_object(genie::get_root());
		lua_insert(L, 1);
		return hier_get_child(L);
	}

	LFUNC(glob_get_objects)
	{
		lua::push_object(genie::get_root());
		lua_insert(L, 1);
		return hier_get_children(L);
	}

	LFUNC(glob_get_systems)
	{
		lua::push_object(genie::get_root());
		lua_insert(L, 1);
		return hier_get_children_by_type<System>(L);
	}

	LGLOBALS(
	{
		LM(get_object, glob_get_object),
		LM(get_objects, glob_get_objects),
		LM(get_systems, glob_get_systems)
	});

	//
	// Node
	//

	// CONSTRUCTOR
	// Creates a new blank Node and registers it with GENIE
	// ARGS: name <string>, verilog module name <string>
	// RETURNS: node <userdata>
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

	// Creates a new Port of a given network type and direction and adds it to the Node
	// ARGS: SELF, port name <string>, port nettype <string>, direction <string>
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

	// Defines (and/or sets) a parameter
	// ARGS: SELF<Node>, param name <string>, [param value <string>]
	LFUNC(node_def_param)
	{
		auto self = lua::check_object<Node>(1);
		std::string parmname = luaL_checkstring(L, 2);
		
		// case-insensitive
		util::str_makeupper(parmname);

		bool exists = self->has_param(parmname);
		bool provide_val = !lua_isnoneornil(L, 3);

		ParamBinding* parm = exists? self->get_param(parmname) : 
			self->add_param(new ParamBinding(parmname));
		
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

	LCLASS(Node, "Node",
	{
		LM(__tostring, hier_get_path),
		LM(get_name, hier_get_name),
		LM(get_hier_path, hier_get_path),
		LM(get_parent, hier_get_parent),
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

	// CONSTRUCTOR: Creates a new System with the given name and registers it in GENIE's Hierarchy
	// ARGS: system name <string>, topology function <functoin>
	// RETURNS: the system <userdata>
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

	// Instantiates a Node (a child of the root) within a System
	// ARGS: SELF, instance name <string>, prototype path <string/object>
	// RETURNS: the new node <userdata>
	LFUNC(system_add_node)
	{
		System* sys = check_object<System>(1);
		std::string instname = luaL_checkstring(L, 2);
		auto prototype = check_obj_or_str_hierpath<Node>(L, 3);

		luaL_argcheck(L, prototype, 3, "node not found");

		HierObject* newobj = prototype->instantiate();

		newobj->set_name(instname);
		sys->add_object(newobj);

		lua::push_object(newobj);

		return 1;
	}

	// Creates split nodes
	// ARGS: SELF<System>, name<string>
	// RETURNS: split node
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

	// Creates merge nodes
	// ARGS: SELF<System>, name<string>
	// RETURNS: merge node
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
		auto src = check_obj_or_str_hierpath<Port>(L, 2, sys);
		auto sink = check_obj_or_str_hierpath<Port>(L, 3, sys);
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
		Port* src = nargs >= 3 ? check_obj_or_str_hierpath<Port>(L, 2, sys) : nullptr;
		Port* sink = nargs >= 3 ? check_obj_or_str_hierpath<Port>(L, 3, sys) : nullptr;

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

	// Exports a Port of a Node contained inside this System. Gives it a name and creates a link
	// to it.
	// ARGS: SELF<System>, port to export<Port>, exported port name<string>
	// RETURNS: exported port<Port>
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

	LCLASS(System, "System",
	{
		LM(__tostring, hier_get_path),
		LM(get_name, hier_get_name),
		LM(add_node, system_add_node),
		LM(add_port, node_add_port),
		LM(get_object, hier_get_child),
		LM(get_objects, hier_get_children),
		LM(get_nodes, hier_get_children_by_type<Node>),
		LM(get_ports, hier_get_children_by_type<Port>),
		LM(add_link, system_add_link),
		LM(get_links, system_get_links),
		LM(def_param, node_def_param),
		LM(add_split, system_add_split),
		LM(add_merge, system_add_merge),
		LM(make_export, system_make_export)
	},
	{
		LM(new, system_new)
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
		lua::push_object(link->get_src());

		return 1;
	}

	// Gets a link's sink object
	// ARGS: SELF
	// RETURNS: object <userdata> or nil
	LFUNC(link_get_sink)
	{
		auto link = lua::check_object<Link>(1);
		lua::push_object(link->get_sink());

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

	// Adds a direct child link
	// ARGS: SELF, other link <userdata>
	// RETURNS: SELF
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

	// Gets all child links of a certain type
	// ARGS: SELF, nettype <string>
	// RETURNS: child links <array<userdata>>
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

	// Gets all parent links of a certain type
	// ARGS: SELF, nettype <string>
	// RETURNS: parent links <array<userdata>>
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

	LCLASS(Link, "Link",
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

	// ARGS: SELF<Port>, role<string>, tag<string>, verilog name<string>, width<number/string>
	//  or
	// ARGS: SELF<Port>, role<string>, verilog name<string>, width<number/string>
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
		std::string widthexpr = luaL_checkstring(L, has_tag? 5 : 4);

		// Get Role ID and Role Definition
		Network* netdef = Network::get(self->get_type());
		
		SigRoleID sigrole_id = netdef->role_id_from_name(sigrole_str);
		if (sigrole_id == ROLE_INVALID)
			throw HierException(self, "invalid signal role: " + sigrole_str);

		auto& sigrole = netdef->get_sig_role(sigrole_id);

		// Access Port's Node's vlog module and create a new vlog port if it does not exist yet
		Node* node = self->get_node();
		auto vinfo = as_a<vlog::NodeVlogInfo*>(node->get_hdl_info());
		vlog::Port* vport = vinfo->get_port(vlog_portname);

		if (!vport)
		{
			auto vpdir = vlog::Port::make_dir(self->get_dir(), sigrole.get_sense());
			vport = new vlog::Port(vlog_portname, widthexpr, vpdir);
			vinfo->add_port(vport);
		}

		// Add a new signal role binding, bound to the entire verilog port
		self->add_role_binding(sigrole_id, tag, new vlog::VlogStaticBinding(vlog_portname));

		return 0;
	}

	LCLASS(Port, "Port",
	{
		LM(__tostring, hier_get_path),
		LM(get_name, hier_get_name),
		LM(get_parent, hier_get_parent),
		LM(get_hier_path, hier_get_path),
		LM(get_type, net_get_type<Port>),
		LM(get_dir, net_get_dir<Port>),
		LM(get_links, net_get_links),
		LM(add_signal, port_add_signal)
	});

	//
	// RSPort
	//

	// Creates and adds a new linkpoint to a RS portdef
	// ARGS: SELF, lp name <string>, lp encoding <string>, lp type <string>
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
	LFUNC(rsport_get_topo_port)
	{
		// Use -1 to mean 'top of stack' because we may be
		// called directly (through C) from rslp_get_topo_port
		auto self = lua::check_object<RSPort>(-1);

		lua::push_object(self->get_topo_port());
		return 1;
	}

	// Gets this RS port's associated RS port (= itself)
	// ARGS: SELF
	// RETURNS: SELF
	LFUNC(rsport_get_rs_port)
	{
		auto self = lua::check_object<RSPort>(1);
		(void)self;
		return 1;
	}

	// Sets this RS port's associated clock port name
	// ARGS: SELF, portname <string>
	// RETURNS: nil
	LFUNC(rsport_set_clock_port_name)
	{
		auto self = lua::check_object<RSPort>(1);
		const char* clkportnm = luaL_checkstring(L, 2);

		self->set_clock_port_name(clkportnm);

		return 0;
	}

	LCLASS(RSPort, "RSPort",
	{
		LM(__tostring, hier_get_path),
		LM(get_name, hier_get_name),
		LM(get_parent, hier_get_parent),
		LM(get_type, net_get_type<Port>),
		LM(get_dir, net_get_dir<Port>),
		LM(get_links, net_get_links),

		LM(add_linkpoint, rsport_add_linkpoint),
		LM(get_rs_port, rsport_get_rs_port),
		LM(get_topo_port, rsport_get_topo_port),
		LM(add_signal, port_add_signal),
		LM(set_clock_port_name, rsport_set_clock_port_name)
	});

	//
	// RSLinkpoint
	//

	// Gets the linkpoint's associated Topo port, which belongs to
	// the parent RS port of this linkpoint.
	// ARGS: SELF
	// RETURNS: TOPO port <userdata>
	LFUNC(rslp_get_topo_port)
	{
		auto self = lua::check_object<RSLinkpoint>(1);
		lua::push_object(RSPort::get_rs_port(self)->get_topo_port());
		return 1;
	}

	// Gets the linkpoint's parent RS port
	// ARGS: SELF
	// RETURNS: RS port <userdata>
	LFUNC(rslp_get_rs_port)
	{
		auto self = lua::check_object<RSLinkpoint>(1);
		lua::push_object(self->get_parent());
		return 1;
	}

	LCLASS(RSLinkpoint, "RSLinkpoint",
	{
		LM(__tostring, hier_get_path),
		LM(get_name, hier_get_name),
		LM(get_hier_path, hier_get_path),
		LM(get_links, net_get_links),

		LM(get_rs_port, rslp_get_rs_port),
		LM(get_topo_port, rslp_get_topo_port)
	});
}

