#pragma once

#include "prop_macros.h"
#include "port.h"
#include "hdl.h"
#include "genie/port.h"

namespace genie
{
namespace impl
{
    class ClockPort : public Port
    {
    public:
        ClockPort(const std::string& name, genie::Port::Dir dir);
        ClockPort(const ClockPort&);
        ~ClockPort() = default;

        Port* instantiate() const override;
        void resolve_params(ParamResolver&) override;

        PROP_GET_SET(binding, const hdl::PortBindingRef&, m_binding);

    protected:
        hdl::PortBindingRef m_binding;
    };

    class ResetPort : public Port
    {
    public:
        ResetPort(const std::string& name, genie::Port::Dir dir);
        ResetPort(const ResetPort&);
        ~ResetPort() = default;

        Port* instantiate() const override;
        void resolve_params(ParamResolver&) override;

        PROP_GET_SET(binding, const hdl::PortBindingRef&, m_binding);

    protected:
        hdl::PortBindingRef m_binding;
    };
}
}