#pragma once

#include "prop_macros.h"
#include "port.h"
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

    class PortRS : virtual public genie::PortRS, public Port
    {
    public:
        void add_signal(Role role, const std::string& sig_name, 
            const std::string& width = "1") override;
        void add_signal(Role role, const std::string& tag,
			const std::string& sig_name, const std::string& width) override;
        void add_signal_ex(Role role, const HDLPortSpec&, const HDLBindSpec&) override;
        void add_signal_ex(Role role, const std::string& tag, 
            const HDLPortSpec&, const HDLBindSpec&) override;

    public:
		static void init();

        PortRS(const std::string& name, genie::Port::Dir dir);
		PortRS(const std::string& name, genie::Port::Dir dir, const std::string& clk_port_name);
        PortRS(const PortRS&);
        ~PortRS();

        PortRS* clone() const override;
        void resolve_params(ParamResolver&) override;
		Port* export_port(const std::string& name, NodeSystem*) override;
		PortType get_type() const override { return PORT_RS; }

        PROP_GET_SET(clock_port_name, const std::string&, m_clk_port_name);
		PROP_GET_SET(domain_id, unsigned, m_domain_id);

        std::vector<PortRSSub*> get_subports() const;
		std::vector<PortRSSub*> get_subports(genie::PortRS::Role role);
		PortRSSub* get_subport(genie::PortRS::Role role, const std::string& tag = "");
		PortRSSub* add_subport(genie::PortRS::Role role, const hdl::PortBindingRef& bnd);
		PortRSSub* add_subport(genie::PortRS::Role role, const std::string& tag,
			const hdl::PortBindingRef& bnd);
		PortClock* get_clock_port() const;

    protected:
        std::string m_clk_port_name;
		unsigned m_domain_id;
    };

	extern PortType PORT_RS_SUB;

    class PortRSSub : public SubPortBase
    {
    public:
		static void init();

        PortRSSub(const std::string& name, genie::Port::Dir dir);
        PortRSSub(const PortRSSub&);
        ~PortRSSub() = default;

        PortRSSub* clone() const override;
        void resolve_params(ParamResolver&) override;
		Port* export_port(const std::string& name, NodeSystem*) override;
		PortType get_type() const override { return PORT_RS_SUB; }

		PROP_GET_SET(role, const genie::PortRS::Role, m_role);
		PROP_GET_SET(tag, const std::string&, m_tag);

    protected:
		// TODO: move this to generic subport base class?
		genie::PortRS::Role m_role;
		std::string m_tag;
    };
}
}