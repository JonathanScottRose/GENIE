#pragma once

#include "prop_macros.h"
#include "port.h"
#include "hdl.h"
#include "genie/port.h"

namespace genie
{
namespace impl
{
	class NodeSystem;

	extern PortType PORT_CLOCK;

    class PortClock : public Port
    {
    public:
		static void init();

        PortClock(const std::string& name, genie::Port::Dir dir);
        PortClock(const PortClock&);
        ~PortClock() = default;

        Port* instantiate() const override;
        void resolve_params(ParamResolver&) override;
		genie::Port* export_port(const std::string&, NodeSystem*) override;
		PortType get_type() const override { return PORT_CLOCK; }

        PROP_GET_SET(binding, const hdl::PortBindingRef&, m_binding);

		PortClock* get_connected_clk_port(Node* context) const;

    protected:
        hdl::PortBindingRef m_binding;
    };

	extern PortType PORT_RESET;

    class PortReset : public Port
    {
    public:
		static void init();

        PortReset(const std::string& name, genie::Port::Dir dir);
        PortReset(const PortReset&);
        ~PortReset() = default;

        Port* instantiate() const override;
        void resolve_params(ParamResolver&) override;
		genie::Port* export_port(const std::string&, NodeSystem*) override;
		PortType get_type() const override { return PORT_RESET; }

        PROP_GET_SET(binding, const hdl::PortBindingRef&, m_binding);

    protected:
        hdl::PortBindingRef m_binding;
    };
}
}