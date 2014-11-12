#pragma once

// Fix for GCC's piss-poor handling of std::regex. Uses boost's instead and injects into std namespace.
// Visual C++ has the upper hand standards-wise, for once

#ifdef _WIN32
#include <regex>
#else
#include <boost/regex.hpp>
namespace std
{
	using boost::regex;
	using boost::smatch;
	using boost::regex_search;
	using boost::regex_match;
}
#endif

