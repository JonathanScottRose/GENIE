#pragma once

#include <string>
#include <vector>


struct Globals
{
	// Defaults defined here
	bool dump_p2p_graph = false;
	bool dump_topo_graph = false;

	static Globals* inst()
	{
		static Globals s_inst;
		return &s_inst;
	}

private:
	Globals() = default;
};
