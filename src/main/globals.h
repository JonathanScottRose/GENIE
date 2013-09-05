#pragma once

#include <string>
#include <vector>

typedef std::vector<std::string> StringVec;

struct Globals
{
	StringVec input_files;
	StringVec component_path;

	// Set defaults
	Globals()
	{
		component_path.push_back(".");
	}

	static Globals* inst()
	{
		static Globals the_inst;
		return &the_inst;
	}
};
