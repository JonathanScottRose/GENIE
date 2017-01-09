#pragma once

#include <vector>
#include "lua.h"

namespace genie
{
namespace lua_if
{
	// Macros for function/class definition
	#define ESC_PAREN(...) __VA_ARGS__
	#define LFUNC(name) int name(lua_State* L)
	#define LM(name,func) {#name, func}
    #define LCLASS(cls, ...) priv::ClassReg< cls > s_##cls##_reg (#cls, __VA_ARGS__)
    #define LSUBCLASS(cls, supers, ...) priv::ClassReg< cls, ESC_PAREN supers > s_##cls##_reg (#cls, __VA_ARGS__)
	#define LGLOBALS(...) priv::GlobalsReg s_globals_reg(__VA_ARGS__)	
	
	// Class registration and C++ interop
	void push_object(void* inst);
	template<class T> T* check_object(int narg);
	template<class T> T* is_object(int narg);
	template<class T> std::string obj_typename(T* = nullptr);

	// Init/shutdown
	using ArgsVec = std::vector<std::pair<std::string, std::string>>;
	void init(const ArgsVec&);
	void shutdown();

	// utility
	lua_State* get_state();
	void exec_script(const std::string& filename);
	void pcall_top(int nargs, int nret);
	int make_ref();
	void push_ref(int ref);
	void free_ref(int ref);
	void stackdump();
	void lerror(const std::string& what);
}
}

#include "lua_if_impl.h"