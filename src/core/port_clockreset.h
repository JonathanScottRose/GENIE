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
		static SigRoleType CLOCK;

		static void init();

        PortClock(const std::string& name, genie::Port::Dir dir);
		PortClock(const std::string& name, genie::Port::Dir dir, const hdl::PortBindingRef& bnd);
        PortClock(const PortClock&);
        ~PortClock() = default;

        PortClock* clone() const override;
		Port* export_port(const std::string&, NodeSystem*) override;
		PortType get_type() const override { return PORT_CLOCK; }

		const hdl::PortBindingRef& get_binding();
		void set_binding(const hdl::PortBindingRef&);

		PortClock* get_connected_clk_port(Node* context) const;
		PortClock* get_driver(Node* context) const;
    };

	extern PortType PORT_RESET;

    class PortReset : public Port
    {
    public:
		static SigRoleType RESET;

		static void init();

        PortReset(const std::string& name, genie::Port::Dir dir);
		PortReset(const std::string& name, genie::Port::Dir dir, const hdl::PortBindingRef& bnd);
        PortReset(const PortReset&);
        ~PortReset() = default;

        PortReset* clone() const override;
		Port* export_port(const std::string&, NodeSystem*) override;
		PortType get_type() const override { return PORT_RESET; }

		const hdl::PortBindingRef& get_binding();
		void set_binding(const hdl::PortBindingRef&);
    };
}
}