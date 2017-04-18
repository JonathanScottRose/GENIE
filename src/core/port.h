#pragma once

#include "hierarchy.h"
#include "hdl.h"
#include "genie/port.h"

namespace genie
{
namespace impl
{
	class NodeSystem;

	class PortTypeInfo
	{
	public:
		PROP_GET_SET(name, const std::string&, m_short_name);
		PROP_GET_SET(id, PortType, m_id);
		PROP_GET_SET(default_network, NetType, m_default_network);

		PortTypeInfo() = default;
		virtual ~PortTypeInfo() = default;

		virtual Port* create_port(const std::string& name, genie::Port::Dir dir) = 0;

	protected:
		std::string m_short_name;
		PortType m_id;
		NetType m_default_network;
	};

    class Port : virtual public genie::Port, public HierObject
    {
    public:
        virtual Dir get_dir() const override;
        
    public:
        Port(const std::string& name, Dir dir);
        virtual ~Port();

        virtual void resolve_params(ParamResolver&) = 0;
		virtual Port* export_port(const std::string& name, NodeSystem* context) = 0;
		virtual PortType get_type() const = 0;

        Node* get_node() const;
		genie::Port::Dir get_effective_dir(Node* contain_ctx) const;

    protected:
		Port(const Port&);

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
		// TODO: put signal role stuff in here?
        hdl::PortBindingRef m_binding;
    };
}
}