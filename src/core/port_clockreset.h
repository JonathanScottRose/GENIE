#pragma once

#include "prop_macros.h"
#include "port.h"
#include "hdl.h"
#include "genie/port.h"

namespace genie
{
namespace impl
{
    class PortClock : public Port
    {
    public:
        PortClock(const std::string& name, genie::Port::Dir dir);
        PortClock(const PortClock&);
        ~PortClock() = default;

        Port* instantiate() const override;
        void resolve_params(ParamResolver&) override;

        PROP_GET_SET(binding, const hdl::PortBindingRef&, m_binding);

    protected:
        hdl::PortBindingRef m_binding;
    };

    class PortReset : public Port
    {
    public:
        PortReset(const std::string& name, genie::Port::Dir dir);
        PortReset(const PortReset&);
        ~PortReset() = default;

        Port* instantiate() const override;
        void resolve_params(ParamResolver&) override;

        PROP_GET_SET(binding, const hdl::PortBindingRef&, m_binding);

    protected:
        hdl::PortBindingRef m_binding;
    };
}
}