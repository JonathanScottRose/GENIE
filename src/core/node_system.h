#pragma once

#include "node.h"

namespace genie
{
namespace impl
{
    class NodeSystem : public Node
    {
    public:
        NodeSystem(const std::string& name);

        Node* instantiate(const std::string&) override;

    protected:
        NodeSystem(const NodeSystem&, const std::string&);
    };
}
}