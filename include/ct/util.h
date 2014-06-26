#pragma once

#include <string>
#include <fstream>
#include <algorithm>

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
}