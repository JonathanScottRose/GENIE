#pragma once

#include "genie/common.h"
#include "genie/hierarchy.h"
#include "genie/networks.h"
#include "genie/connections.h"
#include "genie/parameter.h"

namespace genie
{
	class Node;
	class Port;
	class System;
	class HierRoot;

	class Port : public HierObject
	{
	public:
		typedef std::vector<RoleBinding*> RoleBindings;

		Port(Dir, NetType);
		Port(const Port&);
		virtual ~Port();

		Node* get_node() const;
		bool is_export() const;

		// Get port type and direction
		PROP_GET(type, NetType, m_type);
		PROP_GET(dir, Dir, m_dir);

		// Connectivity
		typedef List<NetType> NetTypes;
		NetTypes get_connectable_networks() const;
		bool is_connectable(NetType) const;
		bool is_connected(NetType) const;
		Endpoint* get_endpoint(NetType, LinkFace) const;
		Endpoint* get_endpoint(NetType, HierObject*) const;

		// Manage signal role bindings
		const RoleBindings& get_role_bindings() { return m_role_bindings; }
		RoleBinding* add_role_binding(SigRoleID, const std::string&, HDLBinding*);
		RoleBinding* add_role_binding(SigRoleID, HDLBinding*);
		RoleBinding* add_role_binding(const std::string&, const std::string&, HDLBinding*);
		RoleBinding* add_role_binding(const std::string&, HDLBinding*);
		RoleBinding* add_role_binding(RoleBinding*);
		RoleBindings get_role_bindings(SigRoleID);
		RoleBinding* get_role_binding(SigRoleID, const std::string&);
		RoleBinding* get_role_binding(SigRoleID);
		RoleBinding* get_matching_role_binding(RoleBinding*);
		bool has_role_binding(SigRoleID);
		bool has_role_binding(SigRoleID, const std::string&);

		HierObject* instantiate() override;

	protected:
		typedef std::pair<Endpoint*, Endpoint*> EndpointsEntry;
		typedef std::unordered_map<NetType, EndpointsEntry> EndpointsMap;

		void set_connectable(NetType, Dir);
		void set_unconnectable(NetType);
		static Endpoint* get_ep_by_face(const EndpointsEntry&, LinkFace);
		static void set_ep_by_face(EndpointsEntry&, LinkFace, Endpoint*);

		const SigRole& get_role_def(SigRoleID) const;

		Dir m_dir;
		NetType m_type;
		RoleBindings m_role_bindings;
		EndpointsMap m_endpoints;
	};

	class Node : public HierObject
	{
	public:
		typedef List<Port*> Ports;

		Node();
		Node(const Node&);
		virtual ~Node() = default;

		Ports get_ports(NetType) const;
		Ports get_ports() const;
		Port* get_port(const std::string& name) const;
		Port* add_port(Port*);
		void delete_port(const std::string& name);

		HierObject* instantiate() override;

		PROP_DICT(Params, param, ParamBinding);
		PROP_GET(exp_resolver, const expressions::NameResolver&, m_resolv);

	protected:
		expressions::NameResolver m_resolv;
		void init_resolv();
	};

	class System : public Node
	{
	public:
		typedef List<Link*> Links;
		typedef List<NetType> NetTypes;
		typedef List<Node*> Nodes;
		typedef List<Port*> Exports;
		typedef List<HierObject*> Objects;

		System();
		virtual ~System();

		//HierObject* instantiate() override;

		// Link-related
		NetTypes get_net_types() const;

		Links get_links() const;
		Links get_links(NetType) const;
		Links get_links(Port* src, Port* sink) const;
		Links get_links(Port* src, Port* sink, NetType net) const;
		Link* connect(Port* src, Port* sink);
		Link* connect(Port* src, Port* sink, NetType net);
		void disconnect(Port* src, Port* sink);
		void disconnect(Port* src, Port* sink, NetType net);
		void disconnect(Link*);
		void splice(Link* orig, Port* new_sink, Port* new_src);

		// Access children
		Objects get_objects() const;
		Nodes get_nodes() const;
		Exports get_exports() const;

		void add_object(HierObject*);
		HierObject* get_object(const HierPath& path) const;
		void delete_object(const HierPath& path);

		// Debug
		void write_dot(const std::string& filename, NetType nettype);
		
	protected:
		NetType find_auto_net_type(Port*, Port*) const;
		void get_eps(Port*&, Port*&, NetType, Endpoint*&, Endpoint*&) const;
		bool verify_common_parent(HierObject*, HierObject*, bool&, bool&) const;

		std::unordered_map<NetType, Links> m_links;
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
}