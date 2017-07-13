#pragma once

#include <cstdint>
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
	class PrimDB;

	using NetType = uint16_t;
	constexpr NetType NET_INVALID = std::numeric_limits<NetType>::max();

	using PortType = unsigned;
	constexpr PortType PORT_INVALID = std::numeric_limits<PortType>::max();

	using FieldType = unsigned;
	constexpr FieldType FIELD_INVALID = std::numeric_limits<FieldType>::max();

	using SigRoleType = genie::SigRoleType;
	using SigRoleID = genie::SigRoleID;

	// Module database
	PrimDB* load_prim_db(const std::string& modname, const SmartEnumTable& col_enum,
		const SmartEnumTable& tnode_src_enum, const SmartEnumTable& tnode_sink_enum);
	PrimDB* get_prim_db(const std::string& modname);
	void register_reserved_module(const std::string&);
	bool is_reserved_module(const std::string&);

	// Node management
    Node* get_node(const std::string& name);
    std::vector<NodeSystem*> get_systems();
    void delete_node(Node* node);
	void delete_node(const std::string& name);
    
	// Network management
	NetType register_network(NetworkDef*);
	const NetworkDef* get_network(NetType id);
	const NetworkDef* get_network(const std::string& name);

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