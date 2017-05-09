#pragma once

#include "prop_macros.h"
#include "port.h"
#include "protocol.h"
#include "genie/port.h"

namespace genie
{
namespace impl
{
	class PortClock;
	class NodeSystem;
    class PortRS;
    class PortRSSub;

	extern PortType PORT_RS;

	extern FieldType FIELD_USERDATA;
	extern FieldType FIELD_USERADDR;
	extern FieldType FIELD_XMIS_ID;
	extern FieldType FIELD_EOP;

    class PortRS : virtual public genie::PortRS, public Port
    {
    public:
		void add_signal(const SigRoleID& role,
			const std::string& sig_name, const std::string& width) override;
		void add_signal_ex(const SigRoleID& role,
			const HDLPortSpec&, const HDLBindSpec&) override;
		void set_logic_depth(unsigned) override;

    public:
		struct RoleBinding
		{
			SigRoleID role;
			hdl::PortBindingRef binding;
		};

		static SigRoleType DATA_CARRIER;

		static void init();

        PortRS(const std::string& name, genie::Port::Dir dir);
		PortRS(const std::string& name, genie::Port::Dir dir, const std::string& clk_port_name);
        PortRS(const PortRS&);
        ~PortRS();

        PortRS* clone() const override;
        void resolve_params(ParamResolver&) override;
		Port* export_port(const std::string& name, NodeSystem*) override;
		PortType get_type() const override { return PORT_RS; }
		void reintegrate(HierObject*) override;

		PROP_GET(logic_depth, unsigned, m_logic_depth);
        PROP_GET_SET(clock_port_name, const std::string&, m_clk_port_name);
		PROP_GET_SET(domain_id, unsigned, m_domain_id);
		PROP_GET_SETR(proto, PortProtocol&, m_proto);

		// Future.
		/*
        std::vector<PortRSSub*> get_subports() const;
		std::vector<PortRSSub*> get_subports(SigRoleType type);
		PortRSSub* get_subport(const SigRoleID&);
		PortRSSub* add_subport(const SigRoleID&, const hdl::PortBindingRef& bnd);
		*/

		PortClock* get_clock_port() const;

		// Present and past.
		const std::vector<RoleBinding>& get_role_bindings() const;
		std::vector<RoleBinding> get_role_bindings(SigRoleType type);
		RoleBinding* get_role_binding(const SigRoleID&);
		RoleBinding& add_role_binding(const SigRoleID&, const hdl::PortBindingRef&);

		CarrierProtocol* get_carried_proto() const;
		bool has_field(const FieldID&) const;

    protected:
		unsigned m_logic_depth;
        std::string m_clk_port_name;
		unsigned m_domain_id;
		PortProtocol m_proto;
		std::vector<RoleBinding> m_role_bindings;
    };

	extern PortType PORT_RS_SUB;

    class PortRSSub : public SubPortBase
    {
    public:
		static void init();

        PortRSSub(const std::string& name, genie::Port::Dir dir, const SigRoleID&);
        ~PortRSSub() = default;

        PortRSSub* clone() const override;
		Port* export_port(const std::string& name, NodeSystem*) override;
		PortType get_type() const override { return PORT_RS_SUB; }

		PROP_GET_SET(role, const SigRoleID&, m_role);

    protected:
		// TODO: move this to generic subport base class?
		SigRoleID m_role;
    };
}
}