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

class SmartEnumTable
{
public:
	SmartEnumTable(const char* str)
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

	unsigned size() const
	{
		return _tab.size();
	}

	const char* to_string(unsigned val) const
	{
		return val >= _tab.size() ? nullptr : _tab[val];
	}

	bool from_string(const char* str, unsigned& out) const
	{
		std::string str_ucase(str);
		std::transform(str_ucase.begin(), str_ucase.end(), str_ucase.begin(), ::toupper);

		for (auto it = _tab.begin(); it != _tab.end(); ++it)
		{
			if (strcmp(*it, str_ucase.c_str()) == 0)
			{
				out = it - _tab.begin();
				return true;
			}
		}
		return false;
	}

protected:
	std::vector<const char*> _tab;
	std::string _str;
};


//
//
#define ESC_PAREN(...) __VA_ARGS__

#define SMART_ENUM_EX(name, bonus, ...) \
	class name \
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
		operator name##_e() const { return _val; } \
		const char* to_string() const \
		{ \
			return get_table().to_string((unsigned)_val); \
		} \
		\
		static bool from_string(const char* str, name& out) \
		{ \
			unsigned tmp; \
			bool result = get_table().from_string(str, tmp); \
			out._val = (name##_e)tmp; \
			return result; \
		} \
		\
		static constexpr unsigned size() \
		{ \
			name##_e vals[] = { __VA_ARGS__ }; \
			return sizeof(vals) / sizeof(name##_e); \
		} \
		\
		static SmartEnumTable& get_table() \
		{ \
			static SmartEnumTable tab(#__VA_ARGS__); \
			return tab; \
		} \
	protected: \
		name##_e _val; \
	ESC_PAREN bonus \
	};

#define SMART_ENUM(name, ...) SMART_ENUM_EX(name, (), __VA_ARGS__)
