#pragma once

#include <string>
#include <fstream>
#include <algorithm>

namespace genie
{
namespace impl
{
	namespace util
	{
		// Dynamic cast helper
		template<class T, class TARG>
		T* as_a(TARG* x)
		{
			return dynamic_cast<T*>(x);
		}

		template<class T, class TARG>
		bool is_a(TARG* x)
		{
			return is_a<T>(x) != nullptr;
		}

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
		static void delete_all(T& container)
		{
			for (auto& i : container)
			{
				delete i;
			}
		}

		template<class T>
		static void delete_all_2(T& container)
		{
			for (auto& i : container)
			{
				delete i.second;
			}
		}

		// Do deep copy on container
		template<class T>
		static void copy_all(const T& src, T& dest)
		{
			for (auto i : src)
			{
				using obj_type = typename std::remove_reference<decltype(*i)>::type;
				dest.push_back(new obj_type(*i));
			}
		}

		template<class T>
		static void copy_all_2(const T& src, T& dest)
		{
			for (auto i : src)
			{
				using obj_type = typename std::remove_reference<decltype(*i.second)>::type;
				dest[i.first] = new obj_type(*i.second);
			}
		}

		// Check for existence of items in containers
		template <class T, class V>
		static bool exists(T& container, const V& elem)
		{
			return std::find(container.begin(), container.end(), elem) != container.end();
		}

		template <class T, class V>
		static bool exists_2(T& container, const V& elem)
		{
			return container.count(elem) != 0;
		}

		// Erase from container
		template <class T, class V>
		static void erase(T& container, const V& elem)
		{
			container.erase(std::find(container.begin(), container.end(), elem));
		}

		// Take a key/value type and return just the values
		template <class DEST, class SRC>
		static DEST values(const SRC& src)
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
		static DEST keys(const SRC& src)
		{
			DEST result;
			for (const auto& i : src)
			{
				result.push_back(i.first);
			}
			return result;
		}

		// Wrapper for std::find that takes just the container and assumes begin()/end() for range
		template<class CONT, class V>
		static auto find(CONT& cont, const V& v) -> decltype(std::find(cont.begin(), cont.end(), v))
		{
			return std::find(cont.begin(), cont.end(), v);
		}

		// Cast SCONT=container of A* to a DCONT=container of B*
		template<class DCONT, class SCONT>
		static DCONT container_cast(const SCONT& cont)
		{
			return *(reinterpret_cast<const DCONT*>(&cont));
		}

		// Create a reverse-mapping.
		// Take an associative container K->V
		// And create a new one, mapping V->list(K)
		template <class K, class V>
		static std::unordered_map<V, std::vector<K>> reverse_map(const std::unordered_map<K,V>& in)
		{
			std::unordered_map<V, std::vector<K>> out;
			for (const auto& p : in)
			{
				out[p.second].push_back(p.first);
			}
			return out;
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
			val = std::max(0, val-1);
			while (val)
			{
				val >>= 1;
				result++;
			}
			return result;
		}

		// Int to binary string
		static std::string to_binary(unsigned int x, int bits)
		{
			std::string result;

			while (bits--)
			{
				result = ((x & 1) ? "1" : "0") + result;
				x >>= 1;
			}

			return result;
		}
	}
}
}