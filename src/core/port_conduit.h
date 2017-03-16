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
		PortConduitSub* get_subport(const std::string& tag);

    protected:
    };

    class PortConduitSub : public SubPortBase
    {
    public:
        PortConduitSub(const std::string& name, genie::Port::Dir dir, 
			genie::PortConduit::Role role, const std::string& tag);
        PortConduitSub(const PortConduitSub&) = default;

        Port* instantiate() const override;

		PROP_GET_SET(tag, const std::string&, m_tag);
		PROP_GET_SET(role, genie::PortConduit::Role, m_role);

    protected:
		genie::PortConduit::Role m_role;
		std::string m_tag;
    };
}
}