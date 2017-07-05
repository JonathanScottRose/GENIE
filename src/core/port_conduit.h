#pragma once

#include "port.h"

namespace genie
{
namespace impl
{
	class Port;
	class NodeSystem;
    class PortConduit;

	extern PortType PORT_CONDUIT;

    class PortConduit : virtual public genie::PortConduit, public Port
    {
    public:

    public:
		static void init();

        PortConduit(const std::string& name, genie::Port::Dir dir);
        PortConduit(const PortConduit&);
        ~PortConduit();

        PortConduit* clone() const override;
		Port* export_port(const std::string&, NodeSystem*) override;
		PortType get_type() const override { return PORT_CONDUIT; }

    protected:
    };
}
}