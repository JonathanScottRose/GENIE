#pragma once

#include "genie/node.h"

namespace genie
{
namespace impl
{
    class Node : public genie::Node
    {
    public:
        // Public API
        const std::string& get_name() const override { return m_name; }
        
        // Internal API
        Node(const std::string& name, const std::string& hdl_name);
        virtual ~Node();

        const std::string& get_hdl_name() const { return m_hdl_name; }

        void set_parent(Node*);
        Node* get_parent() const { return m_parent; }

        virtual Node* instantiate(const std::string& name) = 0;

    protected:
        Node(const Node&, const std::string&);

        Node* m_parent;
        std::string m_name;
        std::string m_hdl_name;
    };
}
}