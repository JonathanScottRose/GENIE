#pragma once

#include <string>
#include <vector>

typedef std::vector<std::string> StringVec;

struct Globals
{
	// Set defaults
	Globals()
	{
	}

	static Globals* inst()
	{
		static Globals the_inst;
		return &the_inst;
	}
};
