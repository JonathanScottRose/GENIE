#include <cstring>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "genie/lua/genie_lua.h"
#include "genie/common.h"

using namespace genie;
using namespace lua;

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
        const char* name;       // class (= Lua metatable) name
        std::type_index tindex; // C++ RTTI type index
        FuncList methods;       // list of instance methods
        FuncList statics;       // list of static methods
        TindexList supers;      // list of superclass entries to derive from
        unsigned depth;         // maximum depth of this node in inheritance/superclass hierarchy
        RTTICheckFunc cfunc;    // RTTI cast-check function

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

	LFUNC(s_compareobj)
	{
		auto pa = (const void**)lua_topointer(L, 1);
		auto pb = (const void**)lua_topointer(L, 2);
		lua_pushboolean(L, *pa < *pb? 1 : 0);
		return 1;
	}

	LFUNC(s_equalobj)
	{
		auto pa = (const void**)lua_topointer(L, 1);
		auto pb = (const void**)lua_topointer(L, 2);
		lua_pushboolean(L, *pa == *pb? 1 : 0);
		return 1;
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

	void s_register_class_lua(const ClassRegEntry& entry)
	{
		// Push API table on the stack
		lua_getglobal(s_state, API_TABLE_NAME);

		// Create class table
		lua_newtable(s_state);

		// Add static methods to the class table
		s_register_funclist(entry.statics);

		// Register class table (-1) in the API table(-2) , remove class table from stack
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

		// Add a __classname string with the class's name, which might be useful somehow
		lua_pushstring(s_state, entry.name);
		lua_setfield(s_state, -2, "__classname");

		// Add __lt and __eq metafunctions to enable ordering/comparison
		lua_pushcfunction(s_state, s_compareobj);
		lua_setfield(s_state, -2, "__lt");

		lua_pushcfunction(s_state, s_equalobj);
		lua_setfield(s_state, -2, "__eq");

        // Do not add instance methods here.

		// Instance metatable is all set up, we don't need it on the stack anymore.
		lua_pop(s_state, 1);
	}

    void s_add_instance_methods(const ClassRegEntry& target_entry, const ClassRegEntry& src_entry)
    {
        // Add instance methods to target_entry from src_entry's list of instance methods
        luaL_getmetatable(s_state, target_entry.name);
        s_register_funclist(src_entry.methods);
        lua_pop(s_state, 1);
    }

    unsigned s_register_supers_recursive(const ClassRegEntry& target_entry, const ClassRegEntry& this_entry)
    {
        // target_entry: class to ultimately add methods to, stays constant during recursion
        // this_entry: class we're adding methods from, changes during recursion
        // returns: height of superclass tree

        auto& entries = s_get_reg_entries();
        unsigned max_depth = 0;

        for (auto super_tindex : this_entry.supers)
        {
            // Add instance methods of superclasses first
            auto& super_entry = entries.at(super_tindex);
            unsigned depth = s_register_supers_recursive(target_entry, super_entry);

            // Find highest ancestor tree
            max_depth = std::max(max_depth, depth);
        }

        // Add our instance methods last
        s_add_instance_methods(target_entry, this_entry);

        // Return the maximum number of found superclass levels
        return max_depth + 1;
    }

    void s_register_classes()
    {
        // ASSUMES: global API table is at top of Lua stack

        // Go through each ClassRegEntry
        auto& entries = s_get_reg_entries();
        for (auto& it : entries)
        {
            auto& entry = it.second;

            // Create the class in Lua space
            s_register_class_lua(entry);

            // Add instance methods to the class, including those of its superclasses.
            // Do the superclasses' methods first, so they can be overridden by more-derived classes.
            // Do this with depth-first recursion.
            // This also finds the depth of this class in the inheritance tree.

            unsigned depth = s_register_supers_recursive(entry, entry);
            entry.depth = depth;
        }
    }
}

//
// Public funcs
//

lua_State* lua::get_state()
{
	return s_state;
}

void lua::init(const lua::ArgsVec& argv)
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

void lua::shutdown()
{
	if (s_state) lua_close(s_state);
}

void lua::pcall_top(int nargs, int nret)
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

void lua::exec_script(const std::string& filename)
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

void lua::stackdump()
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

void lua::lerror(const std::string& what)
{
	lua_pushstring(s_state, what.c_str());
	lua_error(s_state);
}

void lua::push_object(Object* inst)
{
	// If null, push nil
	if (!inst)
	{
		lua_pushnil(s_state);
		return;
	}

	// Associate correct metatable with it, based on RTTI type.
    auto inst_tindex = std::type_index(typeid(*inst));

    // Try to find the ClassRegEntry for the RTTI type in the cache
    auto& cache = s_get_class_cache();
    auto it_cache = cache.find(inst_tindex);

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
        ClassRegEntry* best_entry = nullptr;

        auto& entries = s_get_reg_entries();
        for (auto& it_entry : entries)
        {
            auto& entry = it_entry.second;

            // Perfect match? Get outta here. Done.
            if (entry.tindex == inst_tindex)
            {
                best_entry = &entry;
                break;
            }

            // Otherwise, see if it's family-related and keep
            // track of the most-derived matches.
            if (entry.cfunc(inst))
            {
                if (entry.depth > best_depth)
                {
                    best_depth = entry.depth;
                    best_entry = &entry;
                }
            }
        }

        // No related classes found? Throw error
        if (!best_entry)
        {
            lerror("C++ class not registered with Lua interface: " + 
                std::string(typeid(*inst).name()));
        }

        // Class found. Put in cache.
        it_cache = cache.insert(cache.begin(), std::make_pair(inst_tindex, best_entry));
    }

    ClassRegEntry* cached_entry = it_cache->second;

    // We wrap the Object pointer 'inst' in a full userdata
	auto ud = (Object**)lua_newuserdata(s_state, sizeof(Object*), 1);
	*ud = inst;

    // Set this full userdata's metatable to the one associated with the class (by string name)
	luaL_setmetatable(s_state, cached_entry->name);
	
	// leaves userdata on stack as return value
}


//
// Implementations of stuff in lua::priv namespace
//

void lua::priv::create_classreg(const char* name, std::type_index tindex, 
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

Object* lua::priv::to_object(int narg)
{
	auto ud = (Object**)lua_touserdata(s_state, narg);
	if (!ud)
		return nullptr;

	return *ud;
}


