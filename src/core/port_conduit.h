#pragma once

#include "port.h"
#include "genie/port.h"

namespace genie
{
namespace impl
{
    class ConduitPort;
    class ConduitSubPort;

    class ConduitPort : virtual public genie::ConduitPort, virtual public Port
    {
    public:
        void add_signal(Role role, const std::string& tag, const std::string& sig_name, 
            const std::string& width) override;
        void add_signal(Role role, const std::string& tag, 
            const HDLPortSpec&, const HDLBindSpec&) override;

    public:
        ConduitPort(const std::string& name, genie::Port::Dir dir);
        ConduitPort(const ConduitPort&);
        ~ConduitPort();

        Port* instantiate() const override;
        void resolve_params(ParamResolver&) override;

        std::vector<ConduitSubPort*> get_subports() const;

    protected:
    };

    class ConduitSubPort : public SubPortBase
    {
    public:
        ConduitSubPort(const std::string& name, genie::Port::Dir dir);
        ConduitSubPort(const ConduitSubPort&) = default;

        Port* instantiate() const override;

    protected:
    };
}
}