#pragma once

#include "genie/genie.h"
#include "genie/port.h"

namespace genie
{
namespace impl
{
	class Node;
    class NodeSystem;
	class NetworkDef;
	class PortTypeDef;
	class SigRoleDef;

	using NetType = unsigned;
	const NetType NET_INVALID = std::numeric_limits<NetType>::max();

	using PortType = unsigned;
	const PortType PORT_INVALID = std::numeric_limits<PortType>::max();

	using FieldType = unsigned;
	const FieldType FIELD_INVALID = std::numeric_limits<FieldType>::max();

	using SigRoleType = genie::SigRoleType;
	using SigRoleID = genie::SigRoleID;

	// Node management
	void register_reserved_module(const std::string&);
	bool is_reserved_module(const std::string&);
    Node* get_node(const std::string& name);
    std::vector<NodeSystem*> get_systems();
    void delete_node(Node* node);
	void delete_node(const std::string& name);
    
	// Network management
	NetType register_network(NetworkDef*);
	const NetworkDef* get_network(NetType id);

	// Port Types
	PortType register_port_type(PortTypeDef*);
	const PortTypeDef* get_port_type(PortType);

	// Fields
	FieldType register_field();

	// Signal Roles
	SigRoleType register_sig_role(SigRoleDef*);
	const SigRoleDef* get_sig_role(SigRoleType);
	const SigRoleType get_sig_role_from_str(const std::string&);

    // Get options
    genie::FlowOptions& get_flow_options();
    genie::ArchParams& get_arch_params();
}
}