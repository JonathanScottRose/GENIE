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

#include "genie/util.h"
#include "genie/aspects.h"
#include "genie/static_init.h"


// These macros create trivial getters and/or setters for a given member variable
#define PROP_GET(name,type,field) \
	type get_##name () const { return field ; }

#define PROP_GETR(name,type,field) \
	type get_##name () { return field ; }

#define PROP_SET(name,type,field) \
	void set_##name (type name) { field = name ; }

#define PROP_GET_SET(name,type,field) \
	PROP_GET(name,type,field) \
	PROP_SET(name,type,field)

#define PROP_GET_SETR(name,type,field) \
	PROP_GETR(name,type,field) \
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

#define PROP_DICT(name_plur,name_sing,vtype) \
	PROP_DICT_NOSET(name_plur,name_sing,vtype) \
	vtype* add_##name_sing(vtype* v) \
	{ \
	assert (m_##name_sing##s.count(v->get_name()) == 0); \
	m_##name_sing##s[v->get_name()] = v; \
	return v; \
	} \

#define PROP_DICT_NOSET(name_plur,name_sing,vtype) \
	typedef std::unordered_map<std::string, vtype*> name_plur; \
	const name_plur& name_sing##s() const { return m_##name_sing##s; } \
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

namespace genie
{
	// Common typedef of a hashmap keyed by std::string
	template<class T> using StringMap = std::unordered_map<std::string, T>;

	// Wrapper around STL
	template<class T> using List = std::vector<T>;

	// GENIE exception base class
	class Exception : public std::runtime_error
	{
	public:
		Exception(const char* what)
			: std::runtime_error(what) { }
		Exception(const std::string& what)
			: std::runtime_error(what.c_str()) { }
	};
}