#pragma once

#include "hierarchy.h"
#include "hdl.h"
#include "genie/port.h"

namespace genie
{
namespace impl
{
	class NodeSystem;

	class PortTypeDef
	{
	public:
		PROP_GET_SET(name, const std::string&, m_short_name);
		PROP_GET_SET(id, PortType, m_id);
		PROP_GET_SET(default_network, NetType, m_default_network);
		
		void set_roles(const std::initializer_list<const char*>&);
		bool has_role(SigRoleType) const;

		PortTypeDef() = default;
		virtual ~PortTypeDef() = default;

		virtual Port* create_port(const std::string& name, genie::Port::Dir dir) = 0;

	protected:
		std::string m_short_name;
		PortType m_id;
		NetType m_default_network;
		std::vector<const char*> m_roles;
	};

    class Port : virtual public genie::Port, public HierObject, public IInstantiable
    {
    public:
        Dir get_dir() const override;
		void add_signal(const SigRoleID& role,
			const std::string& sig_name, const std::string& width) override;
		void add_signal_ex(const SigRoleID& role,
			const HDLPortSpec&, const HDLBindSpec&) override;

    public:
		struct RoleBinding
		{
			SigRoleID role;
			hdl::PortBindingRef binding;
		};

        Port(const std::string& name, Dir dir);
        virtual ~Port();

		virtual Port* export_port(const std::string& name, NodeSystem* context) = 0;
		virtual PortType get_type() const = 0;

		HierObject* instantiate() const override;

        Node* get_node() const;
		genie::Port::Dir get_effective_dir(Node* contain_ctx) const;
		void resolve_params(ParamResolver&);

		std::vector<RoleBinding>& get_role_bindings();
		std::vector<RoleBinding> get_role_bindings(SigRoleType type);
		RoleBinding* get_role_binding(const SigRoleID&);
		RoleBinding& add_role_binding(const SigRoleID&, const hdl::PortBindingRef&);

    protected:
		Port(const Port&);

        Dir m_dir;
		std::vector<RoleBinding> m_role_bindings;
    };
}
}