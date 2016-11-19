#pragma once

#include "genie/common.h"
#include "genie/lua/lua.h"

namespace genie
{
namespace lua
{
	// Macros for function/class definition
	#define ESC_PAREN(...) __VA_ARGS__
	#define LFUNC(name) int name(lua_State* L)
	#define LM(name,func) {#name, func}
    #define LCLASS(cls, ...) priv::ClassReg< cls > s_##cls##_reg (#cls, __VA_ARGS__)
    #define LSUBCLASS(cls, supers, ...) priv::ClassReg< cls, ESC_PAREN supers > s_##cls##_reg (#cls, __VA_ARGS__)
	#define LGLOBALS(...) priv::GlobalsReg s_globals_reg(__VA_ARGS__)	

	// Init/shutdown
	using ArgsVec = std::vector<std::pair<std::string,std::string>>;
	void init(const ArgsVec&);
	void shutdown();
	
	// Class registration and C++ interop
	void push_object(Object* inst);
	template<class T> T* check_object(int narg);
	template<class T> T* is_object(int narg);
	template<class T> std::string obj_typename(T* = nullptr);

	// utility
	lua_State* get_state();
	void exec_script(const std::string& filename);
	void pcall_top(int nargs, int nret);
	int make_ref();
	void push_ref(int ref);
	void free_ref(int ref);
	void lerror(const std::string& what);
}
}

#include "genie/lua/genie_lua_impl.h"