#pragma once

//
// Private implementations of templated things and data structures for genie_lua.h. Do not use directly.
//

#include "genie/common.h"
#include "genie/lua/lua.h"
#include "genie/lua/lauxlib.h"
#include "genie/lua/static_init.h"

namespace genie
{
namespace lua_if
{
    namespace priv
    {
        // A list of name->luaCFunction pairs
        using FuncList = std::vector<std::pair<const char*, lua_CFunction>>;

        // A function that checks if a given Object is castable to a certain type
        using RTTICheckFunc = std::function<bool(Object*)>;

        // Vector of typeindex
        using TindexList = std::vector<std::type_index>;

        // priv function declarations
        Object* to_object(int narg);
        void create_classreg(const char* name, std::type_index tindex, 
            const RTTICheckFunc& cfunc, const TindexList& supers, const FuncList& methods, 
            const FuncList& statics);
        
        //
        // Called by LCLASS macro to register a class
        //

        // Variadic base case: registration of class T, with an empty
        // list Ts... of superclasses
        template<class T, class... Ts>
        class ClassReg
        {
        public:
            // Called by macro - forward to constructor that takes a supers list,
            // with an empty supers list
            ClassReg(const char* name, const FuncList& methods,
                const FuncList& statics = {})
                : ClassReg(name, methods, statics, {})
            {
            }

        protected:
            // This does the actual registration
            ClassReg(const char* name, const FuncList& methods,
                const FuncList& statics, const TindexList& supers)
            {
                // type_index of T, the class-to-be-registered
                std::type_index this_index = std::type_index(typeid(T));

                // check function: returns true if an object is castable to T*
                RTTICheckFunc cfunc = [](Object* o)
                {
                    return dynamic_cast<T*>(o) != nullptr;
                };

                create_classreg(name, this_index, cfunc, supers, methods, statics);
            }
        };

        // Variadic recursive case: registering class T, with a nonempty
        // list of superclasses S,Ts... where S is the first superclass in said list
        template<class T, class S, class... Ts>
        class ClassReg<T, S, Ts...> : ClassReg<T, Ts...>
        {
        public:
            using ThisClass = ClassReg<T, S, Ts...>;

            // This is actually called by the macro - forward to other constructor
            ClassReg(const char* name, const FuncList& methods, 
                const FuncList& statics = {})
                : ThisClass(name, methods, statics, {})
            {
            }

        protected:
            using SuperClass = ClassReg<T, Ts...>;

            // Peel off class S from supers list, convert it into a typeindex, append it
            // to a vector of typeindices, and forward that vector to a recursive
            // constructor invocation.
            ClassReg(const char* name, const FuncList& methods, 
                const FuncList& statics, TindexList supers)
                : SuperClass(name, methods, statics, append<S>(supers))
            {
            }

        private:
            // Returns copy of vector 'lst' with new item appended at the end
            template<class C>
            TindexList append(const TindexList& lst)
            {
                TindexList result(lst);
                result.push_back(std::type_index(typeid(C)));
                return result;
            }
        };

        // Used for registering globals
        using GlobalsReg = StaticInitBase<FuncList>;
    } // end priv namespace
   
    // Implementations of public functions
    template<class T>
    T* is_object(int narg)
    {
        Object* obj = priv::to_object(narg);
        T* result = as_a<T*>(obj);

        return result;
    }
    
    template<class T>
    std::string obj_typename(T* ptr)
    {
        if (ptr) return typeid(*ptr).name();
        else return typeid(T).name();
    }

    template<class T>
    T* check_object(int narg)
    {
        luaL_checktype(get_state(), narg, LUA_TUSERDATA);

        Object* obj = priv::to_object(narg);
        assert(obj);

        T* result = as_a<T*>(obj);
        if (!result)
        {
            std::string msg = "can't convert to " + 
                obj_typename<T>() + " from " +
                obj_typename(obj);
            luaL_argerror(get_state(), narg, msg.c_str());
        }

        return result;
    }
}
}