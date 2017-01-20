#pragma once

#include "node.h"

namespace genie
{
namespace impl
{
    class NodeSystem : virtual public Node, virtual public System
    {
    public:
        void create_sys_param(const std::string& name) override;

    public:
        NodeSystem(const std::string& name);

        Node* instantiate(const std::string&) override;
        
        std::vector<Node*> get_nodes() const;

    protected:
        NodeSystem(const NodeSystem&, const std::string&);
    };
}
}