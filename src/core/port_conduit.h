#pragma once

#include "port.h"
#include "genie/port.h"

namespace genie
{
namespace impl
{
    class PortConduit;
    class PortConduitSub;

    class PortConduit : virtual public genie::PortConduit, virtual public Port
    {
    public:
        void add_signal(Role role, const std::string& tag, const std::string& sig_name, 
            const std::string& width) override;
        void add_signal(Role role, const std::string& tag, 
            const HDLPortSpec&, const HDLBindSpec&) override;

    public:
        PortConduit(const std::string& name, genie::Port::Dir dir);
        PortConduit(const PortConduit&);
        ~PortConduit();

        Port* instantiate() const override;
        void resolve_params(ParamResolver&) override;

        std::vector<PortConduitSub*> get_subports() const;

    protected:
    };

    class PortConduitSub : public SubPortBase
    {
    public:
        PortConduitSub(const std::string& name, genie::Port::Dir dir);
        PortConduitSub(const PortConduitSub&) = default;

        Port* instantiate() const override;

    protected:
    };
}
}