// Platform-specific code to get path of executable

#include "pch.h"
#include "util.h"

using namespace genie::impl::util;

#ifdef _WIN32
	#include <windows.h>

	std::string genie::impl::util::get_exe_path()
	{
		std::string result(MAX_PATH, '\0');

		// includes filename of actual executable
		DWORD n = GetModuleFileNameA(NULL, 
			const_cast<LPSTR>(result.data()), result.size()+1);

		// remove executable, leaving just dir and slashes
		result.resize(result.find_last_of('\\') + 1);

		return result;
	}
#else // ifdef _WIN32
	#include <unistd.h>
	#include <errno.h>
	#include <limits.h>

	std::string genie::impl::util::get_exe_path()
	{
		std::string result(PATH_MAX, '\0');
		ssize_t n = readlink("/proc/self/exe", 
			const_cast<char*>(result.data()), result.size()+1);

		result.resize(result.find_last_of('/') + 1);
		
		return result;
	}
#endif
