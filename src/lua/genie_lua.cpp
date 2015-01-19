#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "genie/lua/genie_lua.h"
#include "genie/common.h"

using namespace genie;
using namespace lua;

extern volatile int foo;

namespace
{
	struct RTTIBinding
	{
		const char* lua_name;
		RTTICheckFunc check;
		RTTIBinding(const char* x, const RTTICheckFunc& y) 
			: lua_name(x), check(y) { }
	};

	const char* API_TABLE_NAME = "genie";
	lua_State* s_state = nullptr;
	std::vector<RTTIBinding> s_rtti_bindings;

	int s_panic(lua_State* L)
	{
		std::string err = luaL_checkstring(L, -1);
		lua_pop(L, 1);
		throw Exception(err);
		return 0;
	}

	int s_stacktrace(lua_State* L)
	{
		const char* err = luaL_checkstring(L, -1);
		luaL_traceback(L, L, err, 1);
		return 1;
	}

	int s_printclass(lua_State* L)
	{
		lua_pushliteral(L, "userdata");
		return 1;
	}

	void s_register_funclist(const FuncList& flist)
	{
		for (const auto& entry : flist)
		{
			const char* name = entry.first;
			lua_CFunction func = entry.second;

			lua_pushcfunction(s_state, func);
			lua_setfield(s_state, -2, name);
		}
	}

	void s_register_rtti(const char* clsname, const RTTICheckFunc& checkfunc)
	{
		for (auto it = s_rtti_bindings.begin();
			it != s_rtti_bindings.end(); ++it)
		{
			if (!strcmp(clsname, it->lua_name))
			{
				throw Exception("class " + std::string(clsname) + " already registered");
			}
		}

		s_rtti_bindings.emplace_back(clsname, checkfunc);
	}

	void s_register_class(const ClassRegDB::Entry& entry)
	{
		// Push API table on the stack
		lua_getglobal(s_state, API_TABLE_NAME);

		// Create class table
		lua_newtable(s_state);

		// Add static methods to the class table
		s_register_funclist(entry.statics);

		// Register class table in the API table, remove API table from stack
		lua_setfield(s_state, -2, entry.name);
		lua_pop(s_state, 1);

		// Create the registry-based metatable for this class. This will hold
		// all the instance methods, and the __index method that allows
		// instances of this class to automatically access those methods.
		if (luaL_newmetatable(s_state, entry.name) == 0)
			throw Exception("class name already exists: " + std::string(entry.name));

		// Set metatable's __index to point to the metatable itself.
		lua_pushvalue(s_state, -1);
		lua_setfield(s_state, -2, "__index");

		/*
		// Set print handler when someone tries to print() an instance
		lua_pushcfunction(s_state, s_printclass);
		lua_setfield(s_state, -2, "__print");
		*/

		// Now add the instance methods
		s_register_funclist(entry.methods);

		// Instance metatable is all set up, we don't need it on the stack anymore.
		lua_pop(s_state, 1);

		// Register RTTI stuff
		s_register_rtti(entry.name, entry.cfunc);
	}
}

//
// Public funcs
//

lua_State* lua::get_state()
{
	return s_state;
}

void lua::init()
{
	s_state = luaL_newstate();
	lua_atpanic(s_state, s_panic);
	luaL_checkversion(s_state);
	luaL_openlibs(s_state);

	lua_newtable(s_state);
	lua_setglobal(s_state, API_TABLE_NAME);

	for (auto& entry : ClassRegDB::entries())
	{
		s_register_class(entry);
	}
}

void lua::shutdown()
{
	if (s_state) lua_close(s_state);
}

void lua::exec_script(const std::string& filename)
{
	// Error handler functions for lua_pcall(). Push it before
	// luaL_loadfile pushes the loaded code onto the stack
	lua_pushcfunction(s_state, s_stacktrace);

	// Load the file. This pushes one entry on the stack.
	int s = luaL_loadfile(s_state, filename.c_str());
	if (s != LUA_OK)
	{
		std::string err = luaL_checkstring(s_state, -1);
		lua_pop(s_state, 1);
		throw Exception(err);
	}

	// Run the function at the top of the stack (the loaded file), using
	// the error handler function (which is one stack entry previous) if something
	// goes wrong.
	s = lua_pcall(s_state, 0, LUA_MULTRET, lua_absindex(s_state, -2));
	if (s != LUA_OK)
	{
		std::string err = luaL_checkstring(s_state, -1);
		lua_pop(s_state, 1);
		throw Exception(err);
	}
}

void lua::register_func(const char* name, lua_CFunction fptr)
{
	lua_getglobal(s_state, API_TABLE_NAME);
	lua_pushcfunction(s_state, fptr);
	lua_setfield(s_state, -2, name);
	lua_pop(s_state, 1);
}

void lua::register_funcs(luaL_Reg* entries)
{
	lua_getglobal(s_state, API_TABLE_NAME);
	luaL_setfuncs(s_state, entries, 0);
	lua_pop(s_state, 1);
}



int lua::make_ref()
{
	return luaL_ref(s_state, LUA_REGISTRYINDEX);
}

void lua::push_ref(int ref)
{
	lua_rawgeti(s_state, LUA_REGISTRYINDEX, ref);
}

void lua::free_ref(int ref)
{
	luaL_unref(s_state, LUA_REGISTRYINDEX, ref);
}

void lua::lerror(const std::string& what)
{
	lua_pushstring(s_state, what.c_str());
	lua_error(s_state);
}

void lua::push_object(Object* inst)
{
	// Associate correct metatable with it, based on RTTI type
	auto it = s_rtti_bindings.begin();
	auto it_end = s_rtti_bindings.end();
	for ( ; it != it_end; ++it)
	{
		if (it->check(inst))
			break;
	}

	if (it == it_end)
	{
		lerror("C++ class not registered with Lua interface: " + 
			std::string(typeid(*inst).name()));
	}

	auto ud = (Object**)lua_newuserdata(s_state, sizeof(Object*));
	*ud = inst;

	luaL_setmetatable(s_state, it->lua_name);
	
	// leaves userdata on stack
}

Object* lua::check_object_internal(int narg)
{
	auto ud = (Object**)lua_touserdata(s_state, narg);
	if (!ud)
		luaL_argerror(s_state, narg, "not userdata");

	return *ud;
}




/*
#define LFUNC(name) int name(lua_State* L)
#define REG_LFUNC(name) register_func(#name, name)

namespace
{
	// Types
	struct AttribDef
	{
		AttribDef(const std::string _name, bool _mandatory)
			: name(_name), mandatory(_mandatory) { }
		std::string name;
		bool mandatory;
	};
	typedef std::vector<AttribDef> RequiredAttribs;
	typedef std::unordered_map<std::string, std::string> Attribs;

	const std::string API_TABLE_NAME = "ct";
	lua_State* s_state;

	void lerror(const std::string& s)
	{
		lua_pushstring(s_state, s.c_str());
		lua_error(s_state);
	}

	Attribs get_table(lua_State* L)
	{
		Attribs result;

		luaL_checktype(L, -1, LUA_TTABLE);
		lua_pushnil(L);
		while (lua_next(L, -2))
		{
			std::string key = luaL_checkstring(L, -2);
			std::string val = luaL_checkstring(L, -1);
			result[key] = val;
			lua_pop(L, 1);
		}

		return result;
	}

	Attribs parse_attribs(lua_State* L, RequiredAttribs& req_attribs)
	{
		Attribs result;

		luaL_checktype(L, -1, LUA_TTABLE);
		lua_pushnil(L);
		while (lua_next(L, -2))
		{
			std::string key = luaL_checkstring(L, -2);
			std::string val = luaL_checkstring(L, -1);

			// Make sure this attribute is valid
			bool valid = false;
			for (auto& req : req_attribs)
			{
				if (req.name == key)
				{
					result[key] = val;
					valid = true;
				}
			}

			if (!valid) lerror("Invalid property: " + key);
				
			lua_pop(L, 1);
		}

		// Check for missing attributes
		for (auto& i : req_attribs)
		{
			if (i.mandatory && result.count(i.name) == 0)
				lerror("Missing property: " + i.name);
		}

		return result;
	}

	LFUNC(reg_component)
	{
		// args: properties table

		RequiredAttribs req;
		req.emplace_back("name", true);
		req.emplace_back("hier", true);
		auto attrs = parse_attribs(L, req);

		auto comp = new Spec::Component(attrs["name"]);
		comp->set_aspect_val<std::string>(attrs["hier"]);
		Spec::define_component(comp);

		lua_pop(L, 1);
		return 0;
	}

	LFUNC(inst_defparams)
	{
		// args: systemname, instname, { parmname=parmval}, parmname=parmval, ...}

		std::string sysname = luaL_checkstring(L, -3);
		std::string instname = luaL_checkstring(L, -2);
		auto attrs = get_table(L);
		
		auto sys = Spec::get_system(sysname);
		auto inst = (Spec::Instance*)sys->get_object(instname);
		if (inst->get_type() != Spec::SysObjegenie::INSTANCE) lerror("object must be instance");

		for (auto& i : attrs)
		{
			inst->set_param_binding(i.first, i.second);
		}

		lua_pop(L, 3);
		return 0;
	}

	LFUNC(reg_interface)
	{
		// args: componentname, {name=, type=, clock=}
		
		std::string compname = luaL_checkstring(L, -2);

		RequiredAttribs req;
		req.emplace_back("name", true);
		req.emplace_back("type", true);
		req.emplace_back("dir", true);
		req.emplace_back("clock", false);
		auto attrs = parse_attribs(L, req);

		auto comp = Spec::get_component(compname);

		Spec::Interface* iface = Spec::Interface::factory
			(
			attrs["name"],
			Spec::Interface::type_from_string(attrs["type"]),
			Spec::Interface::dir_from_string(attrs["dir"]),
			comp
			);

		if (iface->get_type() == Spec::Interface::DATA)
		{
			if (attrs.count("clock") == 0)
				lerror("Missing clock for interface");

			auto dif = (Spec::DataInterface*)iface;
			dif->set_clock(attrs["clock"]);
		}
		
		comp->add_interface(iface);

		lua_pop(L, 2);
		return 0;
	}

	LFUNC(reg_signal)
	{
		using namespace genie::Spec;

		// args: compname, ifacename, {binding=, type=, width=, usertype=}

		std::string compname = luaL_checkstring(L, -3);
		std::string ifacename = luaL_checkstring(L, -2);
		auto comp = Spec::get_component(compname);
		auto iface = comp->get_interface(ifacename);

		RequiredAttribs req;
		req.emplace_back("binding", true);
		req.emplace_back("type", true);
		req.emplace_back("width", false);
		req.emplace_back("usertype", false);
		auto attrs = parse_attribs(L, req);

		Signal::Type stype = Signal::type_from_string(attrs["type"]);

		bool has_width = attrs.count("width") > 0;
		bool needs_width = stype == Signal::DATA;
		std::string swidth = "1";

		needs_width |= stype == Signal::HEADER;
		needs_width |= stype == Signal::LINK_ID;
		needs_width |= stype == Signal::LP_ID;
		needs_width |= stype == Signal::CONDUIT_IN;
		needs_width |= stype == Signal::CONDUIT_OUT;

		if (needs_width)
		{
			if (!has_width)
				lerror("Missing signal width");

			swidth = attrs["width"];
		}

		bool has_usertype = attrs.count("usertype") > 0;
		bool usertype_allowed = iface->get_type() == Interface::CONDUIT;
		usertype_allowed |= stype == Signal::DATA;
		usertype_allowed |= stype == Signal::HEADER;

		if (!usertype_allowed && has_usertype)
			lerror("Unexpected usertype in signal definition");

		if (attrs.count("binding") == 0)
			lerror("Missing HDL signal name in signal definition");

		Signal* sig = new Signal(stype, swidth);
		sig->set_aspect_val<std::string>(attrs["binding"]);

		if (has_usertype)
			sig->set_subtype(attrs["usertype"]);

		iface->add_signal(sig);

		lua_pop(L, 3);
		return 0;
	}

	LFUNC(reg_instance)
	{
		using namespace genie::Spec;

		// args: sysname, {name, comp}

		RequiredAttribs req;
		req.emplace_back("name", true);
		req.emplace_back("comp", true);
		auto attrs = parse_attribs(L, req);

		std::string sysname = luaL_checkstring(L, -2);
		System* sys = Spec::get_system(sysname);

		auto inst = new Instance(attrs["name"], attrs["comp"], sys);
		sys->add_object(inst);

		lua_pop(L, 2);
		return 0;
	}

	LFUNC(reg_link)
	{
		using namespace genie::Spec;

		// args: sysname, src, dest

		std::string sysname = luaL_checkstring(L, -3);
		std::string src = luaL_checkstring(L, -2);
		std::string dest = luaL_checkstring(L, -1);

		auto sys = Spec::get_system(sysname);

		Link* link = new Link(src, dest);
		sys->add_link(link);

		lua_pop(L, 3);
		return 0;
	}

	LFUNC(reg_export)
	{
		using namespace genie::Spec;

		// args: sysname, {name=, type=}

		std::string sysname = luaL_checkstring(L, -2);
		auto sys = Spec::get_system(sysname);

		RequiredAttribs req;
		req.emplace_back("name", true);
		req.emplace_back("type", true);
		req.emplace_back("dir", true);
		auto attrs = parse_attribs(L, req);

		const std::string& name = attrs["name"];
		Interface::Type iftype = Interface::type_from_string(attrs["type"]);
		Interface::Dir ifdir = Interface::dir_from_string(attrs["dir"]);

		Export* ex = new Export(name, iftype, ifdir, sys);
		sys->add_object(ex);

		lua_pop(L, 2);
		return 0;
	}

	LFUNC(reg_system)
	{
		using namespace genie::Spec;

		// args: sysname

		std::string name = luaL_checkstring(L, 1);
		
		auto sys = new System();
		sys->set_name(name);
		Spec::define_system(sys);

		lua_pop(L, 1);
		return 0;
	}

	LFUNC(reg_linkpoint)
	{
		using namespace genie::Spec;

		// args: compname, ifacename, {name=, type=, encoding=}

		std::string compname = luaL_checkstring(L, -3);
		std::string ifacename = luaL_checkstring(L, -2);

		auto comp = Spec::get_component(compname);
		auto iface = comp->get_interface(ifacename);

		RequiredAttribs req;
		req.emplace_back("name", true);
		req.emplace_back("type", true);
		req.emplace_back("encoding", true);
		auto attrs = parse_attribs(L, req);

		Linkpoint::Type type = Linkpoint::type_from_string(attrs["type"]);

		Linkpoint* lp = new Linkpoint(attrs["name"], type, (DataInterface*)iface);
		lp->set_aspect_val<std::string>(attrs["encoding"]);

		iface->add_linkpoint(lp);

		lua_pop(L, 3);
		return 0;
	}

	LFUNC(eval_expression)
	{
		// args: expression (str), param binding (table)
		std::string expr_str = luaL_checkstring(L, 1);
		luaL_checktype(L, 2, LUA_TTABLE);

		// get key/value pairs. recycle "Attribs" for this - not intended purpose
		Attribs params = get_table(L);

		Expression expr(expr_str);

		int result = expr.get_value([&] (const std::string param)
		{
			const auto& it = params.find(param);
			if (it == params.end())
			{
				lerror("Unknown parameter " + param + " while evaluating expression " + expr_str);
			}

			return Expression(it->second);
		});

		lua_pushinteger(L, result);
		return 1;
	}

	LFUNC(create_topo_node)
	{
		Spec::TopoNode* node = new Spec::TopoNode();

		// args: sysname, nodename, nodetype
		std::string sysname = luaL_checkstring(L, 1);
		node->name = luaL_checkstring(L, 2);
		node->type = luaL_checkstring(L, 3);

		Spec::System* sys = Spec::get_system(sysname);
		sys->get_topology()->add_node(node);

		lua_pop(L, 3);
		return 0;
	}

	LFUNC(create_topo_edge)
	{
		// args: sysname, srcnode, destnode, { _={src=, dest=}, ... }
		std::string sysname = luaL_checkstring(L, 1);
		std::string srcnodename = luaL_checkstring(L, 2);
		std::string destnodename = luaL_checkstring(L, 3);
		luaL_checktype(L, 4, LUA_TTABLE);

		Spec::System* sys = Spec::get_system(sysname);
		Spec::TopoNode* srcnode = sys->get_topology()->get_node(srcnodename);
		Spec::TopoNode* destnode = sys->get_topology()->get_node(destnodename);

		Spec::TopoEdge* edge = new Spec::TopoEdge();
		edge->src = srcnodename;
		edge->dest = destnodename;

		lua_pushnil(L);
		while (lua_next(L, 4))
		{
			lua_getfield(L, -1, "src");
			lua_getfield(L, -2, "dest");

			edge->links.emplace_back(Spec::LinkTarget(luaL_checkstring(L, -2)),
				Spec::LinkTarget(luaL_checkstring(L, -1)));

			lua_pop(L, 3);
		}

		sys->get_topology()->add_edge(edge);
		srcnode->outs.push_back(edge);
		destnode->ins.push_back(edge);

		lua_pop(L, 4);
		return 0;
	}

	int s_panic(lua_State* L)
	{
		std::string err = luaL_checkstring(L, -1);
		lua_pop(L, 1);
		throw Exception(err);
		return 0;
	}

	int s_stacktrace(lua_State* L)
	{
		const char* err = luaL_checkstring(L, -1);
		luaL_traceback(L, L, err, 1);
		return 1;
	}

	void register_funcs()
	{
		REG_LFUNC(reg_component);
		REG_LFUNC(inst_defparams);
		REG_LFUNC(reg_interface);
		REG_LFUNC(reg_signal);
		REG_LFUNC(reg_instance);
		REG_LFUNC(reg_link);
		REG_LFUNC(reg_export);
		REG_LFUNC(reg_system);
		REG_LFUNC(reg_linkpoint);
		REG_LFUNC(eval_expression);
		REG_LFUNC(create_topo_node);
		REG_LFUNC(create_topo_edge);
	}
}

lua_State* lua::get_state()
{
	return s_state;
}

void lua::init()
{
	s_state = luaL_newstate();
	lua_atpanic(s_state, s_panic);
	luaL_checkversion(s_state);
	luaL_openlibs(s_state);

	lua_newtable(s_state);
	lua_setglobal(s_state, API_TABLE_NAME.c_str());

	register_funcs();
}

void lua::shutdown()
{
	lua_close(s_state);
}

void lua::exec_script(const std::string& filename)
{
	lua_pushcfunction(s_state, s_stacktrace);

	int s = luaL_loadfile(s_state, filename.c_str());
	if (s != 0)
	{
		std::string err = luaL_checkstring(s_state, -1);
		lua_pop(s_state, 1);
		throw Exception(err);
	}
	
	s = lua_pcall(s_state, 0, LUA_MULTRET, lua_absindex(s_state, -2));
	//lua_call(s_state, 0, LUA_MULTRET);
	if (s != 0)
	{
		std::string err = luaL_checkstring(s_state, -1);
		lua_pop(s_state, 1);
		throw Exception(err);
	}
}

void lua::register_func(const std::string& name, lua_CFunction fptr)
{
	lua_getglobal(s_state, API_TABLE_NAME.c_str());
	lua_pushcfunction(s_state, fptr);
	lua_setfield(s_state, -2, name.c_str());
	lua_pop(s_state, 1);
}

int lua::make_ref()
{
	return luaL_ref(s_state, LUA_REGISTRYINDEX);
}

void lua::push_ref(int ref)
{
	lua_rawgeti(s_state, LUA_REGISTRYINDEX, ref);
}

void lua::free_ref(int ref)
{
	luaL_unref(s_state, LUA_REGISTRYINDEX, ref);
}
*/