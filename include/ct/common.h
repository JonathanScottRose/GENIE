#pragma once

#include <cassert>
#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <forward_list>
#include <list>
#include <fstream>
#include <type_traits>
#include <memory>

#define PROP_GET(name,type,field) \
	type get_##name () const { return field ; }

#define PROP_SET(name,type,field) \
	void set_##name (type name) { field = name ; }

#define PROP_GET_SET(name,type,field) \
	PROP_GET(name,type,field) \
	PROP_SET(name,type,field)

#define PROP_DICT(name_plur,name_sing,vtype) \
	typedef std::unordered_map<std::string, vtype*> name_plur; \
	const name_plur& name_sing##s() { return m_##name_sing##s; } \
	void add_##name_sing(vtype* v) \
	{ \
		assert (m_##name_sing##s.count(v->get_name()) == 0); \
		m_##name_sing##s[v->get_name()] = v; \
	} \
	vtype* get_##name_sing(const std::string& name) \
	{ \
		if (m_##name_sing##s.count(name) == 0) return nullptr; \
		else return m_##name_sing##s[name]; \
	} \
	bool has_##name_sing(const std::string& name) \
	{ \
		return (m_##name_sing##s.count(name) != 0); \
	} \
	protected: \
		name_plur m_##name_sing##s; \
	public:
	
namespace ct
{	
	namespace Util
	{
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
				typedef std::remove_reference<decltype(*i)>::type obj_type;
				dest.push_back(new obj_type(*i));
			}
		}

		template<class T>
		void copy_all_2(const T& src, T& dest)
		{
			for (auto i : src)
			{
				typedef std::remove_reference<decltype(*i.second)>::type obj_type;
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

	// An abstract class with nothing but a virtual destructor. Used for
	// hiding implementation-specific things.
	class ImplAspect
	{
	public:
		virtual ~ImplAspect() {}
	};

	// Allows objects to have many, named, implementation-specific thingies inside
	class HasImplAspect
	{
	public:
		HasImplAspect() : m_impl(nullptr) {}
		virtual ~HasImplAspect() { }
		ImplAspect* get_impl() { return m_impl.get(); }
		void set_impl(ImplAspect* impl) { m_impl = std::shared_ptr<ImplAspect>(impl); }
	protected:
		std::shared_ptr<ImplAspect> m_impl;
	};

	class HasImplAspects
	{
	public:
		typedef std::unordered_map<std::string, ImplAspect*> Aspects;
		virtual ~HasImplAspects() { Util::delete_all_2(m_aspects); }
		ImplAspect* get_impl(const std::string& aspect)
		{
			assert(m_aspects.count(aspect) > 0);
			return m_aspects[aspect];
		}
		void set_impl(const std::string& aspect, ImplAspect* impl)
		{
			m_aspects[aspect] = impl;
		}
		void declare_impl(const std::string& aspect)
		{
			set_impl(aspect, nullptr);
		}
	protected:
		Aspects m_aspects;
	};

	// ConnecTool exception base class
	class Exception : public std::exception
	{
	public:
		Exception(const char* what)
			: std::exception(what) { }
		Exception(const std::string& what)
			: std::exception(what.c_str()) { }
	};
}