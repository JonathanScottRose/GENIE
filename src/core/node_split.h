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
        static NodeSplit* get_prototype();
        
        // Create a new one
        NodeSplit(const std::string& name);

        // Generic copy of an existing one
        Node* instantiate(const std::string& name) override;

    protected:
        NodeSplit();
        NodeSplit(const NodeSplit&, const std::string& name);
    };
}
}