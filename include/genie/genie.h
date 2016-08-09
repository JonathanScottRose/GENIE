#pragma once

#include "genie/hierarchy.h"
#include "genie/structure.h"
#include "genie/networks.h"

namespace genie
{
    struct FlowOptions
    {
        bool dump_dot = false;
        std::string dump_dot_network;

        bool force_full_merge = false;

        bool topo_opt = false;
        std::vector<std::string> topo_opt_systems;

        bool no_mdelay = false;
    };

	// Initialize library
	void init();

    // Get options
    FlowOptions& options();

    // Do everything
    void flow_main();
    void print_stats(System* sys);

	// Get hierarchy root
	HierRoot* get_root();
}

