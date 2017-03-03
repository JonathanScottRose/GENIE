#pragma once

#include <string>
#include <vector>
#include <algorithm>

namespace util
{
	// String upper/lowercasing
	static std::string& str_makelower(std::string& str)
	{
		// Transforms in-place
		std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
	}

	static std::string& str_makeupper(std::string& str)
	{
		// Transforms in-place
		std::transform(str.begin(), str.end(), str.begin(), ::toupper);
        return str;
	}

	static std::string str_tolower(const std::string& str)
	{
		// Returns new string
        std::string result(str);
		return str_makelower(result);
	}

	static std::string str_toupper(const std::string& str)
	{
		// Returns new string
        std::string result(str);
		return str_makeupper(result);
	}

	// Parse a string into an enum (case-insensitive)
	// Return true if successful, result goes in *out
	// (might change this to map later)
	template<class E>
	static bool str_to_enum(const std::vector<std::pair<E, const char*>>& table, 
		const std::string& str, E* out)
	{
		std::string ustr = str_toupper(str);
		for (const auto& entry : table)
		{
			std::string utest = str_toupper(entry.second);
			if (utest == ustr)
			{
				*out = entry.first;
				return true;
			}
		}

		return false;
	}

	// Parse enum to string, returns reference
	template<class E>
	static const char* enum_to_str(const std::vector<std::pair<E, const char*>>& table, 
		E val)
	{
		for (const auto& entry : table)
		{
			if (entry.first == val)
			{
				return entry.second;
			}
		}

		assert(false);
		return nullptr;
	}

	// Log base 2
	static int log2(int val)
	{
		int result = 0;
		val = std::max(0, val-1);
		while (val)
		{
			val >>= 1;
			result++;
		}
		return result;
	}
}
