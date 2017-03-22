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
        void add_signal(Role role, const std::string& sig_name, 
            const std::string& tag, const std::string& width = "1") override;
        void add_signal(Role role, const HDLPortSpec&, const HDLBindSpec&) override;
        void add_signal(Role role, const std::string& tag, 
            const HDLPortSpec&, const HDLBindSpec&) override;

    public:
		static void init();

        PortRS(const std::string& name, genie::Port::Dir dir);
        PortRS(const PortRS&);
        ~PortRS();

        Port* instantiate() const override;
        void resolve_params(ParamResolver&) override;
		genie::Port* export_port(const std::string& name, NodeSystem*) override;
		PortType get_type() const override { return PORT_RS; }

        PROP_GET_SET(clock_port_name, const std::string&, m_clk_port_name);

        std::vector<PortRSSub*> get_subports() const;
		std::vector<PortRSSub*> get_subports(genie::PortRS::Role role);
		PortRSSub* get_subport(genie::PortRS::Role role, const std::string& tag = "");
		PortClock* get_clock_port() const;

    protected:
        std::string m_clk_port_name;
    };

	extern PortType PORT_RS_SUB;

    class PortRSSub : public SubPortBase
    {
    public:
		static void init();

        PortRSSub(const std::string& name, genie::Port::Dir dir);
        PortRSSub(const PortRSSub&);
        ~PortRSSub() = default;

        Port* instantiate() const override;
        void resolve_params(ParamResolver&) override;
		genie::Port* export_port(const std::string& name, NodeSystem*) override;
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