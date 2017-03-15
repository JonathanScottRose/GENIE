#pragma once

#include "node.h"

namespace genie
{
namespace impl
{
    class NodeUser : virtual public Node, virtual public Module
    {
    public:
		genie::Link* create_internal_link(genie::PortRS* src, genie::PortRS* sink,
			unsigned latency) override;

    public:
        NodeUser(const std::string& name, const std::string& hdl_name);

        Node* instantiate(const std::string&) override;

    protected:
        NodeUser(const NodeUser&, const std::string&);
    };
}
}