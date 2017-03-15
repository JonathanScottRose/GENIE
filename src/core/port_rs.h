#pragma once

#include "prop_macros.h"
#include "port.h"
#include "genie/port.h"

namespace genie
{
namespace impl
{
    class PortRS;
    class PortRSField;

    class PortRS : virtual public genie::PortRS, virtual public Port
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
        PortRS(const std::string& name, genie::Port::Dir dir);
        PortRS(const PortRS&);
        ~PortRS();

        Port* instantiate() const override;
        void resolve_params(ParamResolver&) override;

        PROP_GET_SET(clk_port_name, const std::string&, m_clk_port_name);

        std::vector<PortRSField*> get_field_ports() const;

    protected:
        std::string m_clk_port_name;
    };

    class PortRSField : public SubPortBase
    {
    public:
        PortRSField(const std::string& name, genie::Port::Dir dir);
        PortRSField(const PortRSField&);
        ~PortRSField() = default;

        Port* instantiate() const override;
        void resolve_params(ParamResolver&) override;

    protected:
        // Field stuff
    };
}
}