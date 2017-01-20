#pragma once

#include "node.h"

namespace genie
{
namespace impl
{
    class NodeUser : virtual public Node, virtual public Module
    {
    public:

    public:
        NodeUser(const std::string& name, const std::string& hdl_name);

        Node* instantiate(const std::string&) override;

    protected:
        NodeUser(const NodeUser&, const std::string&);
    };
}
}