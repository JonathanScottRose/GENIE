#pragma once

#include "hierarchy.h"
#include "hdl.h"
#include "genie/node.h"

namespace genie
{
namespace impl
{
	class NodeParam;
    class ParamResolver;

    class Node : virtual public genie::Node, public HierObject
    {
    public:
        // Public API
        const std::string& get_name() const override;
        
        void set_int_param(const std::string& parm_name, int val) override;
        void set_int_param(const std::string& parm_name, const std::string& expr) override;
        void set_str_param(const std::string& parm_name, const std::string& str) override;
        void set_lit_param(const std::string& parm_name, const std::string& str) override;

        genie::Port* create_clock_port(const std::string& name, genie::Port::Dir dir, 
            const std::string& hdl_sig);
        genie::Port* create_reset_port(const std::string& name, genie::Port::Dir dir, 
            const std::string& hdl_sig);
        genie::ConduitPort* create_conduit_port(const std::string& name, genie::Port::Dir dir);
        genie::RSPort* create_rs_port(const std::string& name, genie::Port::Dir dir, 
            const std::string& clk_port_name);

    public:
        // Internal API
        virtual Node* instantiate(const std::string& name) = 0;

        Node(const std::string& name, const std::string& hdl_name);
        virtual ~Node();

        const std::string& get_hdl_name() const { return m_hdl_name; }
        Node* get_parent_node() const;
        hdl::HDLState& get_hdl_state() { return m_hdl_state; }

        using Params = std::unordered_map<std::string, NodeParam*>;
        void resolve_params();
        NodeParam* get_param(const std::string& name);
        const Params& get_params() const { return m_params; }
        
    protected:
        Node(const Node&, const std::string&);

		void set_param(const std::string& name, NodeParam* param);

        std::string m_hdl_name;
		Params m_params;
        hdl::HDLState m_hdl_state;
    };
}
}