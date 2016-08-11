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

        bool detailed_stats = false;

        bool force_full_merge = false;

        bool topo_opt = false;
        std::vector<std::string> topo_opt_systems;

        bool no_mdelay = false;
    };

    struct ArchParams
    {
        unsigned lutsize = 6;
        unsigned lutram_width = 20;
        unsigned lutram_depth = 32;
    };

	// Initialize library
	void init();

    // Get options
    FlowOptions& options();
    ArchParams& arch_params();

    // Do everything
    void flow_main();
    void print_stats(System* sys);

	// Get hierarchy root
	HierRoot* get_root();
}

