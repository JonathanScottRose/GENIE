#pragma once

#include "network.h"
#include "node.h"
#include "genie/genie.h"

namespace genie
{
namespace impl
{
    class NodeSystem;

	// Node management
    void register_builtin(Node* node);
    Node* get_node(const std::string& name);
    std::vector<NodeSystem*> get_systems();
    void delete_node(Node* node);
	void delete_node(const std::string& name);
    
	// Network management
	NetType register_network(Network*);
	const Network* get_network(NetType id);

    // Get options
    genie::FlowOptions& get_flow_options();
    genie::ArchParams& get_arch_params();
}
}