#pragma once

#include "node.h"

namespace genie
{
namespace impl
{
    class NodeSplit : public Node
    {
    public:
        static void init();
        
        // Create a new one
		NodeSplit();

        // Generic copy of an existing one
        Node* instantiate() override;

    protected:
        NodeSplit(const NodeSplit&);
    };
}
}