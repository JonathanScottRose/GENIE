#pragma once

#include <string>
#include <vector>
#include <unordered_map>

struct Globals
{
	// Defaults defined here
	bool dump_p2p_graph = false;
	bool dump_topo_graph = false;
	bool register_merge = false;
	std::unordered_map<std::string, std::string> global_params;

	static Globals* inst()
	{
		static Globals s_inst;
		return &s_inst;
	}

private:
	Globals() = default;
};
