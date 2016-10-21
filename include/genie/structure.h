#pragma once

#include "genie/common.h"
#include "genie/hierarchy.h"
#include "genie/networks.h"
#include "genie/connections.h"
#include "genie/parameter.h"
#include "genie/metrics.h"

namespace genie
{
	class Node;
	class Port;
	class System;
	class HierRoot;
	class NodeHDLInfo;
	
	class Port : public HierObject
	{
	public:
		typedef std::vector<RoleBinding*> RoleBindings;

		Port(Dir, NetType);
		Port(const Port&);
		virtual ~Port();

		// Get the parent node (may not be direct parent, as this port could be a sub-port)
		Node* get_node() const;

		// Get the most-ancestral parent Port
		Port* get_primary_port() const;

		// Is this node's parent a System (ie, is this node an Export)?
		bool is_export() const;

		// Get port type and direction
		PROP_GET(type, NetType, m_type);
		PROP_GET(dir, Dir, m_dir);

		// Connectivity
		typedef List<NetType> NetTypes;
		NetTypes get_connectable_networks() const;
		bool is_connectable(NetType) const;
		bool is_connected(NetType) const;
		void set_max_links(NetType, Dir, int);
		Endpoint* get_endpoint(NetType, LinkFace) const;	// Explicit inner/outer-facing endpoint
		Endpoint* get_endpoint_sysface(NetType) const;		// Get endpoint that faces inside the system
		Port* locate_port(Dir, NetType) override;

		// Manage signal role bindings
		void clear_role_bindings();
		RoleBinding* get_matching_role_binding(RoleBinding*);
		const RoleBindings& get_role_bindings() { return m_role_bindings; }
		RoleBinding* add_role_binding(SigRoleID, const std::string&, HDLBinding*);
		RoleBinding* add_role_binding(SigRoleID, HDLBinding*);
		RoleBinding* add_role_binding(const std::string&, const std::string&, HDLBinding*);
		RoleBinding* add_role_binding(const std::string&, HDLBinding*);
		RoleBinding* add_role_binding(RoleBinding*);
		RoleBindings get_role_bindings(SigRoleID);
		RoleBinding* get_role_binding(SigRoleID, const std::string&);
		RoleBinding* get_role_binding(SigRoleID);
		bool has_role_binding(SigRoleID);
		bool has_role_binding(SigRoleID, const std::string&);

	protected:
		typedef std::pair<Endpoint*, Endpoint*> EndpointsEntry;
		typedef std::unordered_map<NetType, EndpointsEntry> EndpointsMap;

		// Can be called by Port implementation code
		void set_connectable(NetType, Dir);
		void set_unconnectable(NetType);

		static Endpoint* get_ep_by_face(const EndpointsEntry&, LinkFace);
		static void set_ep_by_face(EndpointsEntry&, LinkFace, Endpoint*);

		Dir m_dir;
		NetType m_type;
		RoleBindings m_role_bindings;
		EndpointsMap m_endpoints;
	};

	class NodeHDLInfo
	{
	public:
		NodeHDLInfo();
		virtual ~NodeHDLInfo() = default;

		virtual NodeHDLInfo* instantiate() = 0;

		PROP_GET_SET(node, Node*, m_node);

	protected:
		Node* m_node;
	};

	class Node : public HierObject
	{
	public:
		typedef List<Link*> Links;
		typedef List<Port*> Ports;
		typedef List<NetType> NetTypes;

		Node();
		Node(const Node&);
		virtual ~Node();

        virtual bool is_interconnect() const;

		HierObject* instantiate() override;

		// Hack: should be part of generic event handling system
		virtual void do_post_carriage();

		// Ports
		Ports get_ports(NetType) const;
		Ports get_ports() const;
		Port* get_port(const std::string& name) const;
		Port* add_port(Port*);
		void delete_port(const std::string& name);

		// Parameters
		PROP_DICT(Params, param, ParamBinding);
		List<ParamBinding*> get_params(bool are_bound);
		ParamBinding* define_param(const std::string&);
		ParamBinding* define_param(const std::string&, const Expression&);
		expressions::NameResolver get_exp_resolver();

		// HDL Info
		PROP_GET(hdl_info, NodeHDLInfo*, m_hdl_info);
		void set_hdl_info(NodeHDLInfo*);

		// Link-related
		NetTypes get_net_types() const;

		Links get_links() const;
		Links get_links(NetType) const;
		Links get_links(HierObject* src, HierObject* sink) const;
		Links get_links(HierObject* src, HierObject* sink, NetType net) const;
		Link* connect(HierObject* src, HierObject* sink);
		Link* connect(HierObject* src, HierObject* sink, NetType net);
		void disconnect(HierObject* src, HierObject* sink);
		void disconnect(HierObject* src, HierObject* sink, NetType net);
		void disconnect(Link*);
		Link* splice(Link* orig, HierObject* new_sink, HierObject* new_src);

        // Metrics
        virtual AreaMetrics get_area_usage() const;

		// Debug
		void write_dot(const std::string& filename, NetType nettype);

	protected:
		NetType find_auto_net_type(HierObject*, HierObject*) const;
		void get_eps(HierObject*&, HierObject*&, NetType, Endpoint*&, Endpoint*&) const;

		NodeHDLInfo* m_hdl_info;
		std::unordered_map<NetType, Links> m_links;
	};

	class System : public Node
	{
	public:
		typedef List<Node*> Nodes;
		typedef List<Port*> Exports;
		typedef List<HierObject*> Objects;

		System();
		System(const System&) = delete;
		~System();

		// Access children
		Objects get_objects() const;
		Nodes get_nodes() const;
		Exports get_exports() const;
		void delete_object(const HierPath& path);

		// Hack: should be part of generic event handling system
		void do_post_carriage() override;
	};

	class HierRoot : public HierObject
	{
	public:
		HierRoot();
		~HierRoot();

		const std::string& get_name() const override;
		void set_name(const std::string&, bool allow_reserved) override { }
		HierObject* instantiate() override { return nullptr; }

		List<System*> get_systems();
		List<Node*> get_non_systems();
	};

    class AAutoGen : public Aspect
    {
    };
}