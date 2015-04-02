#pragma once

#include <string>
#include <vector>


struct Globals
{
public:
	bool dump_dot = false;
	std::string dump_dot_network;
	
	static Globals* inst()
	{
		static Globals s_inst;
		return &s_inst;
	}

private:
	Globals() = default;
};
