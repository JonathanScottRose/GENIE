#pragma once

#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

//
// Defines a wrapper around an enum that has string conversion capability.
// SMART_ENUM(MyEnum, A, B, C...) behaves like:
//
// enum class MyEnum
// {
//    A, B, C, ...
// };
//
// The resulting object is a value type that's the same size as an enum, but
// also has a to_string() method and a static from_string() method.
//

//
// Factor out some common functionality (could have been part of the macro)
//
class SmartEnumBase
{
protected:
    // Holds a vector of string values for each enum value.
    // Initialized from a big string, and tokenizes upon construction.
	struct Table
	{
		Table(const char* str)
			: _str(str)
		{
            std::transform(_str.begin(), _str.end(), _str.begin(), ::toupper);

			char* data = const_cast<char*>(_str.data());
			char* tok = strtok(data, " ,");
			while (tok != nullptr)
			{
				_tab.push_back(tok);
				tok = strtok(nullptr, " ,");
			}
		}
		
		std::vector<const char*> _tab;
		std::string _str;
	};
};

//
//
#define ESC_PAREN(...) __VA_ARGS__

#define SMART_ENUM_EX(name, bonus, ...) \
	class name : private SmartEnumBase \
	{ \
	public: \
		enum name##_e { __VA_ARGS__ }; \
		\
		name() = default; \
		name(const name&) = default; \
		name& operator=(const name&) = default; \
		/*bool operator == (const name& e) const { return _val == e._val; }*/ \
		bool operator < (const name& e) const { return _val < e._val; } \
		\
		name(name##_e val) : _val(val) {} \
		operator name##_e() { return _val; } \
		const char* to_string() const \
		{ \
			return table()._tab[(unsigned)_val]; \
		} \
		\
		static bool from_string(const char* str, name& out) \
		{ \
			bool result = false; \
			const auto& tab = table()._tab; \
            std::string str_ucase(str); \
            std::transform(str_ucase.begin(), str_ucase.end(), str_ucase.begin(), ::toupper); \
			\
			for (unsigned i = 0; i < tab.size(); i++) \
			{ \
				if (strcmp(tab[i], str_ucase.c_str()) == 0) \
				{ \
					result = true; \
					out = (name##_e)i; \
					break; \
				} \
			} \
			\
			return result; \
		} \
		\
	protected: \
		static Table& table() \
		{ \
			static Table tab(#__VA_ARGS__); \
			return tab; \
		} \
		\
		name##_e _val; \
	ESC_PAREN bonus \
	};

#define SMART_ENUM(name, ...) SMART_ENUM_EX(name, (), __VA_ARGS__)
