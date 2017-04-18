#pragma once

#include "port.h"

namespace genie
{
namespace impl
{
	class Port;
	class NodeSystem;
    class PortConduit;
    class PortConduitSub;

	extern PortType PORT_CONDUIT;

    class PortConduit : virtual public genie::PortConduit, public Port
    {
    public:
        void add_signal(Role role, const std::string& tag, const std::string& sig_name, 
            const std::string& width) override;
        void add_signal_ex(Role role, const std::string& tag, 
            const HDLPortSpec&, const HDLBindSpec&) override;

    public:
		static void init();

        PortConduit(const std::string& name, genie::Port::Dir dir);
        PortConduit(const PortConduit&);
        ~PortConduit();

        PortConduit* clone() const override;
        void resolve_params(ParamResolver&) override;
		Port* export_port(const std::string&, NodeSystem*) override;
		PortType get_type() const override { return PORT_CONDUIT; }

        std::vector<PortConduitSub*> get_subports() const;
		PortConduitSub* get_subport(const std::string& tag);
		PortConduitSub* add_subport(Role role, const std::string& tag,
			const hdl::PortBindingRef& bnd);

    protected:
    };

	extern PortType PORT_CONDUIT_SUB;

    class PortConduitSub : public SubPortBase
    {
    public:
		static void init();

        PortConduitSub(const std::string& name, genie::Port::Dir dir, 
			genie::PortConduit::Role role, const std::string& tag);
        PortConduitSub(const PortConduitSub&) = default;

        PortConduitSub* clone() const override;
		Port* export_port(const std::string&, NodeSystem*) override;
		PortType get_type() const override { return PORT_CONDUIT_SUB; }

		PROP_GET_SET(tag, const std::string&, m_tag);
		PROP_GET_SET(role, genie::PortConduit::Role, m_role);

    protected:
		genie::PortConduit::Role m_role;
		std::string m_tag;
    };
}
}