#pragma once

#include "prop_macros.h"
#include "port.h"
#include "genie/port.h"

namespace genie
{
namespace impl
{
    class RSPort;
    class FieldPort;

    class RSPort : virtual public genie::RSPort, virtual public Port
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
        RSPort(const std::string& name, genie::Port::Dir dir);
        RSPort(const RSPort&);
        ~RSPort();

        Port* instantiate() const override;
        void resolve_params(ParamResolver&) override;

        PROP_GET_SET(clk_port_name, const std::string&, m_clk_port_name);

        std::vector<FieldPort*> get_field_ports() const;

    protected:
        std::string m_clk_port_name;
    };

    class FieldPort : public SubPortBase
    {
    public:
        FieldPort(const std::string& name, genie::Port::Dir dir);
        FieldPort(const FieldPort&);
        ~FieldPort() = default;

        Port* instantiate() const override;
        void resolve_params(ParamResolver&) override;

    protected:
        // Field stuff
    };
}
}