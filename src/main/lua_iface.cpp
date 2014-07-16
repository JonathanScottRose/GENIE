#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "lua_iface.h"

#include "ct/common.h"
#include "ct/spec.h"
#include "ct/ct.h"

using namespace ct;
using namespace LuaIface;

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
	const std::string GLOBALS_TABLE_NAME = "globals";
	lua_State* s_state;

	void lerror(const std::string& s)
	{
		lua_pushstring(s_state, s.c_str());
		lua_error(s_state);
	}

	void push_member(lua_State* L, const std::string& name, bool allow_nil = false)
	{
		lua_getfield(L, -1, name.c_str());

		if (!allow_nil && lua_isnil(L, -1))
			lerror("table key not found: " + name);
	}

	Attribs parse_table(lua_State* L)
	{
		Attribs result;

		luaL_checktype(L, -1, LUA_TTABLE);
		lua_pushnil(L);
		while (lua_next(L, -2))
		{
			std::string key = luaL_checkstring(L, -2);
			if (lua_type(L, -1) != LUA_TTABLE)
			{
				std::string val = luaL_checkstring(L, -1);
				result[key] = val;
			}
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
			if (lua_type(L, -1) != LUA_TTABLE)
			{
				std::string val = luaL_checkstring(L, -1);
				result[key] = val;
			}
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

	void parse_signal(lua_State* L, Spec::Interface* iface)
	{
		using namespace ct::Spec;

		luaL_checktype(L, -1, LUA_TTABLE);

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
	}

	void parse_linkpoint(lua_State* L, Spec::Interface* iface)
	{
		using namespace ct::Spec;

		luaL_checktype(L, -1, LUA_TTABLE);

		RequiredAttribs req;
		req.emplace_back("name", true);
		req.emplace_back("type", true);
		req.emplace_back("encoding", true);
		auto attrs = parse_attribs(L, req);

		Linkpoint::Type type = Linkpoint::type_from_string(attrs["type"]);

		Linkpoint* lp = new Linkpoint(attrs["name"], type, iface);
		lp->set_aspect_val<std::string>(attrs["encoding"]);

		iface->add_linkpoint(lp);
	}

	Spec::Interface* parse_interface(lua_State* L)
	{
		luaL_checktype(L, -1, LUA_TTABLE);

		auto attrs = parse_table(L);

		Spec::Interface* iface = Spec::Interface::factory
			(
			attrs["name"],
			Spec::Interface::type_from_string(attrs["type"]),
			Spec::Interface::dir_from_string(attrs["dir"])
			);

		if (iface->get_type() == Spec::Interface::DATA)
		{
			if (attrs.count("clock") == 0)
				lerror("Missing clock for interface");

			auto dif = (Spec::DataInterface*)iface;
			dif->set_clock(attrs["clock"]);
		}

		push_member(L, "signals");
		lua_pushnil(L);
		while (lua_next(L, -2))
		{
			parse_signal(L, iface);
			lua_pop(L, 1);
		}
		lua_pop(L, 1);

		push_member(L, "linkpoints");
		lua_pushnil(L);
		while (lua_next(L, -2))
		{
			parse_linkpoint(L, iface);
			lua_pop(L, 1);
		}
		lua_pop(L, 1);

		return iface;
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

	LFUNC(reg_parameter)
	{
		RequiredAttribs req;
		req.emplace_back("comp", true);
		req.emplace_back("name", true);
		auto attrs = parse_attribs(L, req);

		auto comp = Spec::get_component(attrs["comp"]);
		if (!comp)
			lerror("Unknown component: " + attrs["comp"]);

		const std::string& param_name = attrs["name"];

		comp->add_parameter(param_name);		

		return 0;
	}

	LFUNC(inst_defparams)
	{
		// args: systemname, instname, { parmname=parmval}, parmname=parmval, ...}

		std::string sysname = luaL_checkstring(L, -3);
		std::string instname = luaL_checkstring(L, -2);
		auto attrs = parse_table(L);
		
		auto sys = Spec::get_system(sysname);
		auto inst = (Spec::Instance*)sys->get_object(instname);
		if (inst->get_type() != Spec::SysObject::INSTANCE) lerror("object must be instance");

		for (auto& i : attrs)
		{
			inst->set_param_binding(i.first, i.second);
		}

		lua_pop(L, 3);
		return 0;
	}

	LFUNC(reg_interface)
	{
		// args: componentname, table:interface
		
		luaL_checktype(L, -1, LUA_TTABLE);
		std::string compname = luaL_checkstring(L, -2);

		Spec::Interface* iface = parse_interface(L);
		
		auto comp = Spec::get_component(compname);
		comp->add_interface(iface);

		return 0;
	}

	LFUNC(reg_instance)
	{
		using namespace ct::Spec;

		// args: sysname, {name, comp}

		RequiredAttribs req;
		req.emplace_back("name", true);
		req.emplace_back("comp", true);
		auto attrs = parse_attribs(L, req);

		std::string sysname = luaL_checkstring(L, -2);
		System* sys = Spec::get_system(sysname);

		auto inst = new Instance(attrs["name"], attrs["comp"], sys);
		sys->add_object(inst);

		return 0;
	}

	LFUNC(reg_link)
	{
		using namespace ct::Spec;

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
		using namespace ct::Spec;

		// args: sysname, {name=, type=}

		std::string sysname = luaL_checkstring(L, -2);
		auto sys = Spec::get_system(sysname);

		RequiredAttribs req;
		req.emplace_back("name", true);
		auto attrs = parse_attribs(L, req);
		const std::string& name = attrs["name"];
		
		push_member(L, "interface");
		Interface* iface = parse_interface(L);

		Export* ex = new Export(name, iface, sys);
		sys->add_object(ex);

		lua_pop(L, 1);
		return 0;
	}

	LFUNC(reg_system)
	{
		using namespace ct::Spec;

		// args: sysname

		std::string name = luaL_checkstring(L, 1);
		
		auto sys = new System();
		sys->set_name(name);
		Spec::define_system(sys);

		lua_pop(L, 1);
		return 0;
	}

	LFUNC(eval_expression)
	{
		// args: expression (str), param binding (table)
		std::string expr_str = luaL_checkstring(L, 1);
		luaL_checktype(L, 2, LUA_TTABLE);

		// get key/value pairs. recycle "Attribs" for this - not intended purpose
		Attribs params = parse_table(L);

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
		REG_LFUNC(reg_parameter);
		REG_LFUNC(inst_defparams);
		REG_LFUNC(reg_interface);
		REG_LFUNC(reg_instance);
		REG_LFUNC(reg_link);
		REG_LFUNC(reg_export);
		REG_LFUNC(reg_system);
		REG_LFUNC(eval_expression);
		REG_LFUNC(create_topo_node);
		REG_LFUNC(create_topo_edge);
	}
}

lua_State* LuaIface::get_state()
{
	return s_state;
}

void LuaIface::init()
{
	s_state = luaL_newstate();
	lua_atpanic(s_state, s_panic);
	luaL_checkversion(s_state);
	luaL_openlibs(s_state);

	// create table for API
	lua_newtable(s_state);
	lua_pushvalue(s_state, -1); // make copy, because setglobal pops the table off the stack
	
	// register table for API (one copy remains on stack)
	lua_setglobal(s_state, API_TABLE_NAME.c_str());

	// create globals sub-table
	lua_newtable(s_state);

	// register globals sub-table
	lua_setfield(s_state, -2, GLOBALS_TABLE_NAME.c_str());
	
	// pop second copy of API table
	lua_pop(s_state, 1);

	register_funcs();
}

void LuaIface::shutdown()
{
	lua_close(s_state);
}

void LuaIface::exec_script(const std::string& filename)
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

void LuaIface::register_func(const std::string& name, lua_CFunction fptr)
{
	lua_getglobal(s_state, API_TABLE_NAME.c_str());
	lua_pushcfunction(s_state, fptr);
	lua_setfield(s_state, -2, name.c_str());
	lua_pop(s_state, 1);
}

void LuaIface::set_global_param(const std::string& key, const std::string& value)
{
	lua_getglobal(s_state, API_TABLE_NAME.c_str()); // +1
	lua_getfield(s_state, -1, GLOBALS_TABLE_NAME.c_str()); // +1
	lua_pushstring(s_state, value.c_str()); // +1
	lua_setfield(s_state, -2, key.c_str()); // -1
	lua_pop(s_state, 2); // -2
}

int LuaIface::make_ref()
{
	return luaL_ref(s_state, LUA_REGISTRYINDEX);
}

void LuaIface::push_ref(int ref)
{
	lua_rawgeti(s_state, LUA_REGISTRYINDEX, ref);
}

void LuaIface::free_ref(int ref)
{
	luaL_unref(s_state, LUA_REGISTRYINDEX, ref);
}
