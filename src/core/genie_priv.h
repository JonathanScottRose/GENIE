#pragma once

#include "genie/genie.h"

namespace genie
{
namespace impl
{
	class Node;
    class NodeSystem;
	class Network;
	class PortTypeInfo;

	using NetType = unsigned;
	const NetType NET_INVALID = std::numeric_limits<NetType>::max();

	using PortType = unsigned;
	const PortType PORT_INVALID = std::numeric_limits<PortType>::max();

	// Node management
	void register_reserved_module(const std::string&);
	bool is_reserved_module(const std::string&);
    Node* get_node(const std::string& name);
    std::vector<NodeSystem*> get_systems();
    void delete_node(Node* node);
	void delete_node(const std::string& name);
    
	// Network management
	NetType register_network(Network*);
	const Network* get_network(NetType id);

	// Port Types
	PortType register_port_type(PortTypeInfo*);
	const PortTypeInfo* get_port_type(PortType);

    // Get options
    genie::FlowOptions& get_flow_options();
    genie::ArchParams& get_arch_params();
}
}