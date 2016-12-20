#pragma once

#include "genie/genie.h"
#include "node.h"

namespace genie
{
namespace impl
{
    void register_builtin(Node* node);

    Node* get_node(const std::string& name);

    // Get options
    genie::FlowOptions& get_flow_options();
    genie::ArchParams& get_arch_params();
}
}