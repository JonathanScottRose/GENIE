#pragma once

#include "hierarchy.h"
#include "node.h"
#include "hdl.h"
#include "genie/port.h"

namespace genie
{
namespace impl
{
    class Port : virtual public genie::Port, virtual public HierObject
    {
    public:
        virtual const std::string& get_name() const override;
        virtual Dir get_dir() const override;
        
    public:
        Port(const std::string& name, Dir dir);
        Port(const Port&);
        virtual ~Port();

        virtual Port* instantiate() const = 0;
        virtual void resolve_params(ParamResolver&) = 0;

        Node* get_node() const;

    protected:
        Dir m_dir;
    };

    class SubPortBase : public Port
    {
    public:
        SubPortBase(const std::string& name, genie::Port::Dir dir);
        SubPortBase(const SubPortBase&) = default;
        virtual ~SubPortBase() = default;

        void resolve_params(ParamResolver&) override;

        PROP_GET_SET(hdl_binding, const hdl::PortBindingRef&, m_binding);

    protected:
        hdl::PortBindingRef m_binding;
    };
}
}