#pragma once

#include "genie/common.h"
#include "genie/lua/genie_lua_impl.h"

namespace genie
{
namespace lua
{
	// Macros for function/class definition
	#define LFUNC(name) int name(lua_State* L)
	#define LM(name,func) {#name, func}
	#define LCLASS(cls, ...) ClassReg s_##cls##_reg(ClassRegEntryT<cls>(__VA_ARGS__))
	#define LGLOBALS(...) GlobalsReg s_globals_reg(__VA_ARGS__)	

	// Init/shutdown
	void init();
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