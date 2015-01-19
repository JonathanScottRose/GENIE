#pragma once

#include <string>
#include <fstream>
#include <algorithm>

namespace genie
{
	// Useful stuff that belongs outside the Util namespace
	template<class T, class O>
	T as_a_check(O ptr)
	{
		T result = dynamic_cast<T>(ptr);
		if (!result)
		{
			throw Exception("Failed casting " +
				std::string(typeid(O).name()) + " to " +
				std::string(typeid(T).name()));
		}
		return result;
	}

	template<class T, class O>
	T as_a(O ptr)
	{
		return dynamic_cast<T>(ptr);
	}

	template<class T, class O>
	bool is_a(O ptr)
	{
		return dynamic_cast<T>(ptr) != nullptr;
	}

	namespace Util
	{
		// Check if file exists
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

		// Erase from container
		template <class T, class V>
		void erase(T& container, const V& elem)
		{
			container.erase(std::find(container.begin(), container.end(), elem));
		}

		// Take a key/value type and return just the values
		template <class DEST, class SRC>
		DEST values(const SRC& src)
		{
			DEST result;
			for (const auto& i : src)
			{
				result.push_back(i.second);
			}
			return result;
		}

		// Extract just the keys
		template <class DEST, class SRC>
		DEST keys(const SRC& src)
		{
			DEST result;
			for (const auto& i : src)
			{
				result.push_back(i.first);
			}
			return result;
		}

		// String upper/lowercasing
		static void str_makelower(std::string& str)
		{
			// Transforms in-place
			std::transform(str.begin(), str.end(), str.begin(), ::tolower);
		}

		static void str_makeupper(std::string& str)
		{
			// Transforms in-place
			std::transform(str.begin(), str.end(), str.begin(), ::toupper);
		}

		static std::string str_tolower(std::string str)
		{
			// Returns new string
			str_makelower(str);
			return str;
		}

		static std::string str_toupper(std::string str)
		{
			// Returns new string
			str_makeupper(str);
			return str;
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
			while (val)
			{
				val >>= 1;
				result++;
			}
			return result;
		}
	}
}