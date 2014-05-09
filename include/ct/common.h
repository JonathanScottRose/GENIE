#pragma once

#include <cassert>
#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <map>
#include <forward_list>
#include <list>
#include <fstream>
#include <type_traits>
#include <memory>
#include <stdexcept>

// These macros create trivial getters and/or setters for a given member variable
#define PROP_GET(name,type,field) \
	type get_##name () const { return field ; }

#define PROP_SET(name,type,field) \
	void set_##name (type name) { field = name ; }

#define PROP_GET_SET(name,type,field) \
	PROP_GET(name,type,field) \
	PROP_SET(name,type,field)

// Defines a dictionary type which maps strings to a given pointer type.
// The pointed-to type must have a get_name() method which returns const std::string&
// When used in a class declaration, this macro instantiates the dictionary and also defines
// useful functions for adding/getting/removing/querying elements from the dictionary.
// In addition to the value-type, the macro needs singular/plural descriptions of the type
// in order to name the dictionary instance and the accessor methods.
// >>>> MEMORY IS NOT CLEANED UP BY ANY OF THESE FUNCTIONS <<<<
//
// Example:
// PROP_DICT(Things, thing, ThingType)
// 
// makes these members:
//
// typedef std::unordered_map<std:string, ThingType*> Things;
// const Things& things();
// void add_thing(ThingType* v);
// ThingType* get_thing(const std::string& name);
// bool has_thing(const std::string& name);
// void delete_thing(const std::string& name);
//
// The class destructor should iterate over things() and call delete on the pointers.
//
#define PROP_DICT(name_plur,name_sing,vtype) PROP_DICT_BASE(name_plur,name_sing,vtype,std::unordered_map)
#define PROP_SDICT(name_plur,name_sing,vtype) PROP_DICT_BASE(name_plur,name_sing,vtype,std::map)

#define PROP_DICT_BASE(name_plur,name_sing,vtype,container) \
	typedef container<std::string, vtype*> name_plur; \
	const name_plur& name_sing##s() const { return m_##name_sing##s; } \
	void add_##name_sing(vtype* v) \
	{ \
		assert (m_##name_sing##s.count(v->get_name()) == 0); \
		m_##name_sing##s[v->get_name()] = v; \
	} \
	vtype* get_##name_sing(const std::string& name) const \
	{ \
		if (m_##name_sing##s.count(name) == 0) return nullptr; \
		else return m_##name_sing##s.at(name); \
	} \
	bool has_##name_sing(const std::string& name) const \
	{ \
		return (m_##name_sing##s.count(name) != 0); \
	} \
	void delete_##name_sing(const std::string& name) \
	{ \
		delete m_##name_sing##s[name]; \
		m_##name_sing##s.erase(name); \
	} \
	protected: \
		name_plur m_##name_sing##s; \
	public:
	
namespace ct
{	
	namespace Util
	{
		// file exists?
		static bool fexists(const std::string& filename)
		{
			std::ifstream ifile(filename);
			return !ifile.bad();
		}

		// Functions to destroy the objects in a container of pointers. First one
		// is for containers that hold pointers. The second one is for
		// containers that hold pairs, the second members of which are pointers.
		template<class T>
		void delete_all(T& container)
		{
			for (auto& i : container)
			{
				delete i;
			}
		}

		template<class T>
		void delete_all_2(T& container)
		{
			for (auto& i : container)
			{
				delete i.second;
			}
		}

		// Do deep copy on container
		template<class T>
		void copy_all(const T& src, T& dest)
		{
			for (auto i : src)
			{
				typedef typename std::remove_reference<decltype(*i)>::type obj_type;
				dest.push_back(new obj_type(*i));
			}
		}

		template<class T>
		void copy_all_2(const T& src, T& dest)
		{
			for (auto i : src)
			{
				typedef typename std::remove_reference<decltype(*i.second)>::type obj_type;
				dest[i.first] = new obj_type(*i.second);
			}
		}

		// Check for existence of items in containers
		template <class T, class V>
		bool exists(T& container, const V& elem)
		{
			return std::find(container.begin(), container.end(), elem) != container.end();
		}

		template <class T, class V>
		bool exists_2(T& container, const V& elem)
		{
			return container.count(elem) != 0;
		}

		// String lowercasing
		static std::string str_tolower(const char* str)
		{
			std::string result(str);
			std::transform(result.begin(), result.end(), result.begin(), ::tolower);
			return result;
		}

		static std::string str_tolower(const std::string& str)
		{
			return str_tolower(str.c_str());
		}

		// Log base 2
		static int log2(int val)
		{
			int result = 0;
			while (val)
			{
				val >>= 1;
				result++;
			}
			return result;
		}
	}

	// Allows objects to have many, named, implementation-specific thingies inside
	class HasImplAspect
	{
	private:
		struct AspectBase
		{
			virtual ~AspectBase() {};
		};

		template<class T>
		struct Aspect : public AspectBase
		{
			T* ptr;
			~Aspect() { delete ptr;	}
			Aspect(T* _ptr) : ptr(_ptr) {}
		};

		AspectBase* m_aspect;

	public:
		HasImplAspect() : m_aspect(nullptr) {}
		
		template<class T>
		T* get_aspect() { return ((Aspect<T>*)m_aspect)->ptr; }

		template<class T>
		const T& get_aspect_val() { return *(get_aspect<T>()); }

		template<class T>
		void set_aspect(T* impl) { m_aspect = new Aspect<T> (impl); }

		template<class T>
		void set_aspect_val(const T& val) { set_aspect<T>(new T(val)); }
	};

	class HasImplAspects
	{
	private:
		struct AspectBase
		{
			virtual ~AspectBase() {};
		};

		template<class T>
		struct Aspect : public AspectBase
		{
			T* ptr;
			~Aspect() { delete ptr; }
			Aspect(T* _ptr) : ptr(_ptr) {}
		};

		typedef std::unordered_map<std::string, AspectBase*> Aspects;
		Aspects m_aspects;
	public:
		~HasImplAspects() { Util::delete_all_2(m_aspects); }

		template<class T>
		T* get_aspect(const std::string& aspect)
		{
			assert(m_aspects.count(aspect) > 0);
			auto asp = (Aspect<T>*)m_aspects[aspect];
			return asp->ptr;
		}

		template<class T>
		const T& get_aspect_val(const std::string& aspect)
		{
			return *(get_aspect<T>(aspect));
		}

		template<class T>
		void set_aspect(const std::string& aspect, T* impl)
		{
			m_aspects[aspect] = new Aspect<T> (impl);
		}

		template<class T>
		void set_aspect_val(const std::string& aspect, const T& val)
		{
			set_aspect<T>(aspect, new T(val));
		}

		void remove_aspect(const std::string& aspect)
		{
			delete m_aspects[aspect];
			m_aspects.erase(aspect);
		}
	};

	// ConnecTool exception base class
	class Exception : public std::runtime_error
	{
	public:
		Exception(const char* what)
			: std::runtime_error(what) { assert(false); }
		Exception(const std::string& what)
			: std::runtime_error(what) { assert(false); }
	};
}
