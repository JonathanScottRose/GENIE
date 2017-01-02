#pragma once

#include "genie/node.h"
#include "hierarchy.h"

namespace genie
{
namespace impl
{
    class Node : public genie::Node, public HierObject
    {
    public:
        // Public API
        const std::string& get_name() const override { return HierObject::get_name(); }
        
        // Internal API
        Node(const std::string& name, const std::string& hdl_name);
        virtual ~Node();

        const std::string& get_hdl_name() const { return m_hdl_name; }

        virtual Node* instantiate(const std::string& name) = 0;

    protected:
        Node(const Node&, const std::string&);
        std::string m_hdl_name;
    };
}
}