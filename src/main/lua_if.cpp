#include <cstring>
#include <cassert>
#include <unordered_map>
#include <algorithm>
#include "genie/genie.h"
#include "genie/log.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lua_if.h"

using namespace genie;
using namespace genie::lua_if;

namespace
{
	// Forward decl.
	using priv::FuncList;
	using priv::TindexList;
	using priv::GlobalsReg;
    using priv::RTTICheckFunc;

	// Information about a C++ class thas is registered with the Lua system
	struct ClassRegEntry
	{
		const char* name;       // class name
		std::type_index tindex; // C++ RTTI type index
		FuncList methods;       // list of instance methods
		FuncList statics;       // list of static methods
		TindexList supers;      // list of superclass entries to derive from
        unsigned depth;         // maximum depth of this node in inheritance/superclass hierarchy
        RTTICheckFunc cfunc;    // RTTI cast-check function

        FuncList dispatch_table; // Final list of functions for this class, populated later

		// Need constructor because typeindex has no default constructor
        ClassRegEntry(const char* _name, std::type_index _tindex, const RTTICheckFunc& _cfunc, 
            const FuncList& _methods, const FuncList& _statics, const TindexList& _supers)
            : name(_name), tindex(_tindex), methods(_methods), statics(_statics), 
            supers(_supers), depth(0), cfunc(_cfunc)
        {
        }
	};

	// Holds all registered class entries by value, keyed by type index.
	// Retrieved by function call for proper lazy initialization
	using ClassRegEntries = std::unordered_map<std::type_index, ClassRegEntry>;
	ClassRegEntries& s_get_reg_entries()
	{
		static ClassRegEntries s_result;
		return s_result;
	}

    // Matches an RTTI type to a class entry by pointer. Contains a superset of
    // s_get_reg_entries(), because it can contain C++ types that are NOT registered
    // with the system, yet are CASTABLE-DOWN into a registered type.
    // Retrieved by function call for proper lazy initialization
    using ClassRegCache = std::unordered_map<std::type_index, ClassRegEntry*>;
    ClassRegCache& s_get_class_cache()
    {
        static ClassRegCache s_cache;
        return s_cache;
    }

    // Constants, Lua state
	const char* API_TABLE_NAME = "genie";
	const char* API_ARGV_TABLE = "argv";
	lua_State* s_state = nullptr;

    // Lua hooks
	LFUNC(s_panic)
	{
		std::string err = luaL_checkstring(L, -1);
		throw Exception(err);
		return 0;
	}

	LFUNC(s_stacktrace)
	{
		const char* err = luaL_checkstring(L, -1);
		luaL_traceback(L, L, err, 1);
		return 1;
	}

	// Main class member lookup entry point!
	// This gets set as the __index function on the metatable for ALL lightuserdatas
	LFUNC(s_dispatch)
	{
		// 'table' must be light userdata
		if (lua_islightuserdata(L, -2) == 0)
		{
			return 0;
		}

        // Get the object, and get the desired method name
		auto obj = (APIObject*)lua_touserdata(L, -2);
		const char* fname = lua_tostring(L, -1);
		assert(fname);

		// Get the RTTI type of the object
        auto obj_tindex = std::type_index(typeid(*obj));

        auto& cache = s_get_class_cache();
        auto it_cache = cache.find(obj_tindex);

        if (it_cache == cache.end())
        {
            // Didn't find an entry in the cache.
            // What we do now is:
            // 1) Look through all registered classes
            // 1.5) If there's a direct typeid match between a registered class
            //      and *inst, then use that for sure. Otherwise...
            // 2) Find the ones that *inst can be cast to
            // 3) Take the most-derived class found in 2)
            // 4) Use this class. Store it in the cache for next time.

            // 3) Is accomplished by finding the ClassRegEntry with the highest 'depth'
            unsigned best_depth = 0;
            ClassRegEntry* best_class = nullptr;

            auto& classes = s_get_reg_entries();
            for (auto& it_class : classes)
            {
                auto& clazz = it_class.second;

                // Perfect match? Get outta here. Done.
                if (clazz.tindex == obj_tindex)
                {
                    best_class = &clazz;
                    break;
                }

                // Otherwise, see if it's family-related and keep
                // track of the most-derived matches.
                if (clazz.cfunc(obj))
                {
                    if (clazz.depth > best_depth)
                    {
                        best_depth = clazz.depth;
                        best_class = &clazz;
                    }
                }
            }

            // No related classes found? Throw error
            if (!best_class)
            {
                lerror("C++ class not registered with Lua interface: " + 
                    std::string(obj_tindex.name()));
            }

            // Class found. Put in cache.
            it_cache = cache.insert(cache.begin(), std::make_pair(obj_tindex, best_class));
        }

        ClassRegEntry* cached_class = it_cache->second;

        // Search through class for method
        for (auto& entry : cached_class->dispatch_table)
        {
            if (strcmp(entry.first, fname) == 0)
            {
                // Method found, return it
                lua_pushcfunction(L, entry.second);
                return 1;
            }
        }

        // Method not found, return nil
		return 0;
	}

    // Adds a list of CFunctions as key/value pairs to the Lua table at the top of the stack
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

    unsigned s_register_supers_recursive(ClassRegEntry& target_entry, const ClassRegEntry& this_entry)
    {
        // target_entry: class to ultimately add methods to, stays constant during recursion
        // this_entry: class we're adding methods from, changes during recursion
        // returns: height of superclass tree

        auto& entries = s_get_reg_entries();
        unsigned max_depth = 0;

		// First, recurse, THEN do the real work
        for (auto super_tindex : this_entry.supers)
        {
            // Add instance methods of superclasses first
            auto& super_entry = entries.at(super_tindex);
            unsigned depth = s_register_supers_recursive(target_entry, super_entry);

            // Find highest ancestor tree
            max_depth = std::max(max_depth, depth);
        }

        // The real work: add/replace instance methods from this_entry into target_entry
		for (auto& fn : this_entry.methods)
		{
			// fn is the method we'd like to add. see if there's an existing method to replace
			auto it = std::find_if(target_entry.dispatch_table.begin(), 
                target_entry.dispatch_table.end(), 
				[=](const std::pair<const char*, lua_CFunction>& meth) 
			{ 
				return strcmp(meth.first, fn.first) == 0;
			});

			if (it != target_entry.dispatch_table.end())
			{
				// Replace it
				it->second = fn.second;
			}
			else
			{
				target_entry.dispatch_table.push_back(fn);
			}
		}

        // Return the maximum number of found superclass levels
        return max_depth + 1;
    }

    void s_register_classes()
    {
        // Go through each ClassRegEntry
        auto& entries = s_get_reg_entries();
        for (auto& it : entries)
        {
            auto& entry = it.second;

			// Add methods of superclasses to our own
            unsigned depth = s_register_supers_recursive(entry, entry);

            // Record the depth of the inheritance tree.
            // This quantifies how-derived this class is
            entry.depth = depth;

            // Add static functions
            std::copy(entry.statics.begin(), entry.statics.end(), entry.dispatch_table.end());
        }

        // Set s_entry as the global __index handler for all lightuserdata
        lua_pushlightuserdata(s_state, nullptr);

        lua_newtable(s_state);
        lua_pushcfunction(s_state, s_dispatch);
        lua_setfield(s_state, -2, "__index");
        lua_setmetatable(s_state, -2);
        lua_pop(s_state, 1);
    }
}

//
// Public funcs
//

lua_State* lua_if::get_state()
{
	return s_state;
}

void lua_if::init(const lua_if::ArgsVec& argv)
{
	// Create global Lua state
	s_state = luaL_newstate();
	lua_atpanic(s_state, s_panic);
	luaL_checkversion(s_state);
	luaL_openlibs(s_state);

	// Create global API table
	lua_newtable(s_state);
	lua_setglobal(s_state, API_TABLE_NAME);

    // Register all API classes
    s_register_classes();
    
	// Push API table onto the stack and register all global API functions
	lua_getglobal(s_state, API_TABLE_NAME);
	
	for (auto& entry : GlobalsReg::entries())
	{
		s_register_funclist(entry);
	}

	// With API table still on the stack, create the argv table and populate it
	lua_newtable(s_state);
	for (const auto& kv : argv)
	{
		const char* key = kv.first.c_str();
		const char* val = kv.second.c_str();

		lua_pushstring(s_state, val);
		lua_setfield(s_state, -2, key);
	}

	// Add argv table to API table
	lua_setfield(s_state, -2, API_ARGV_TABLE);

	// Pop API table
	lua_pop(s_state, 1);
}

void lua_if::shutdown()
{
	if (s_state) lua_close(s_state);
}

void lua_if::pcall_top(int nargs, int nret)
{
	// Calls the function at the top of the stack.
	
	// First, we insert an element at the bottom of the stack,
	// that element being a reference to the stacktrace function.
	lua_pushcfunction(s_state, s_stacktrace);
	lua_insert(s_state, 1);

	// Top of stack now has function plus args like before.
	// Call the function at the top. Error handler function is at index 1.
	int s = lua_pcall(s_state, nargs, nret, 1);
	if (s != LUA_OK)
	{
		std::string err = luaL_checkstring(s_state, -1);
		lua_pop(s_state, 1);
		lua_remove(s_state, 1);
		throw Exception(err);
	}

	// Remove stacktrace function.
	lua_remove(s_state, 1);
}

void lua_if::exec_script(const std::string& filename)
{
	// Load the file. This pushes one entry on the stack.
	int s = luaL_loadfile(s_state, filename.c_str());
	if (s != LUA_OK)
	{
		std::string err = luaL_checkstring(s_state, -1);
		lua_pop(s_state, 1);
		throw Exception(err);
	}

	// Run the function at the top of the stack (the loaded file)
	pcall_top(0, 0);	
}


int lua_if::make_ref()
{
	return luaL_ref(s_state, LUA_REGISTRYINDEX);
}

void lua_if::push_ref(int ref)
{
	lua_rawgeti(s_state, LUA_REGISTRYINDEX, ref);
}

void lua_if::free_ref(int ref)
{
	luaL_unref(s_state, LUA_REGISTRYINDEX, ref);
}

void lua_if::stackdump()
{
	lua_State* l = s_state;

	int i;
	int top = lua_gettop(l);

	printf("total in stack %d\n", top);

	for (i = 1; i <= top; i++)
	{  /* repeat for each level */
		int t = lua_type(l, i);
		switch (t) {
		case LUA_TSTRING:  /* strings */
			printf("string: '%s'\n", lua_tostring(l, i));
			break;
		case LUA_TBOOLEAN:  /* booleans */
			printf("boolean %s\n", lua_toboolean(l, i) ? "true" : "false");
			break;
		case LUA_TNUMBER:  /* numbers */
			printf("number: %g\n", lua_tonumber(l, i));
			break;
		default:  /* other values */
			printf("%s\n", lua_typename(l, t));
			break;
		}
		printf("  ");  /* put a separator */
	}
	printf("\n");  /* end the listing */

}

void lua_if::lerror(const std::string& what)
{
	lua_pushstring(s_state, what.c_str());
	lua_error(s_state);
}


void lua_if::push_object(APIObject* inst)
{
	// If null, push nil
	if (!inst)
	{
		lua_pushnil(s_state);
	}
	else
	{
		lua_pushlightuserdata(s_state, inst);
	}

	// leaves userdata on stack as return value
}


//
// Implementations of stuff in lua::priv namespace
//

void lua_if::priv::create_classreg(const char* name, std::type_index tindex, 
    const RTTICheckFunc& cfunc, const TindexList& supers, const FuncList& methods, 
    const FuncList& statics)
{
    auto& entries = s_get_reg_entries();

    // Check if already registered
    for (const auto& it : entries)
    {
        const auto& entry = it.second;

        if (it.first == tindex || entry.tindex == tindex || !strcmp(name, entry.name))
        {
            throw Exception("class " + std::string(name) + " already registered");
        }
    }

    // Create new entry
    ClassRegEntry entry(name, tindex, cfunc, methods, statics, supers);
    entries.emplace(std::make_pair(tindex, entry));
}

void lua_if::priv::check_object_fail(int narg, const std::type_index& t)
{
    luaL_argerror(s_state, narg, ("not a " + std::string(t.name())).c_str());
}

APIObject* priv::to_object(int narg)
{
    return (APIObject*)lua_touserdata(s_state, narg);
}

