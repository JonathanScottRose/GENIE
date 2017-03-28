#pragma once

#include "node.h"

namespace genie
{
namespace impl
{
    class NodeMerge : public Node
    {
    public:
        static void init();
        
        // Create a new one
		NodeMerge();

        // Generic copy of an existing one
        Node* instantiate() override;

    protected:
		NodeMerge(const NodeMerge&);

		void init_vlog();
    };
}
}