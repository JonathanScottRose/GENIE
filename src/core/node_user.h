#pragma once

#include "node.h"

namespace genie
{
namespace impl
{
    class NodeUser : virtual public genie::Module, public Node, public IInstantiable
    {
    public:
		genie::Link* create_internal_link(genie::PortRS* src, genie::PortRS* sink,
			unsigned latency) override;

    public:
        NodeUser(const std::string& name, const std::string& hdl_name);

		NodeUser* clone() const override;
        NodeUser* instantiate() const override;

    protected:
        NodeUser(const NodeUser&);
    };
}
}