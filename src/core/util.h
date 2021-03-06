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

        static std::string str_con_cat(const std::string& s)
        {
            return s;
        }

        template<class... Ts>
        static std::string str_con_cat(const std::string& s, Ts... rest)
        {
            return s + "_" + str_con_cat(rest...);
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

		// Ceiling Log base 2. Thanks StackOverflow!
		template<class T>
		static int log2(T val)
		{
			static const unsigned long long t[6] = 
			{
				0xFFFFFFFF00000000ull,
				0x00000000FFFF0000ull,
				0x000000000000FF00ull,
				0x00000000000000F0ull,
				0x000000000000000Cull,
				0x0000000000000002ull
			};

			int y = (((val & (val - 1)) == 0) ? 0 : 1);
			int j = 32;
			int i;

			for (i = 0; i < 6; i++)
			{
				int k = (((val & t[i]) == 0) ? 0 : j);
				y += k;
				val >>= k;
				j >>= 1;
			}

			return y;
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

		// https://stackoverflow.com/questions/2960434/c-population-count-of-unsigned-64-bit-integer-with-a-maximum-value-of-15
		static unsigned popcnt(uint64_t w)
		{
			w -= (w >> 1) & 0x5555555555555555ULL;
			w = (w & 0x3333333333333333ULL) + ((w >> 2) & 0x3333333333333333ULL);
			w = (w + (w >> 4)) & 0x0f0f0f0f0f0f0f0fULL;
			return unsigned((w * 0x0101010101010101ULL) >> 56);
		}

		// Declared in separate file
		extern std::string get_exe_path();
	}
}
}