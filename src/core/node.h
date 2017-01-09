#pragma once

#include "genie/node.h"
#include "hierarchy.h"

namespace genie
{
namespace impl
{
	class NodeParam;

    class Node : public genie::Node, public HierObject
    {
    public:
        // Public API
        const std::string& get_name() const override { return HierObject::get_name(); }
        
		void set_int_param(const std::string& parm_name, int val) override;
		void set_int_param(const std::string& parm_name, const std::string& expr) override;
		void set_str_param(const std::string& parm_name, const std::string& str) override;

        // Internal API
        Node(const std::string& name, const std::string& hdl_name);
        virtual ~Node();

        const std::string& get_hdl_name() const { return m_hdl_name; }

        virtual Node* instantiate(const std::string& name) = 0;

    protected:
        Node(const Node&, const std::string&);

		void set_param(const std::string& name, NodeParam* param);

        std::string m_hdl_name;
		std::unordered_map<std::string, NodeParam*> m_params;
    };
}
}