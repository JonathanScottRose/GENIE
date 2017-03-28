#pragma once

#include "hierarchy.h"
#include "hdl.h"
#include "genie/node.h"

namespace genie
{
namespace impl
{
	class Port;
	class NodeParam;
    class ParamResolver;
	class Link;

    class Node : virtual public genie::Node, public HierObject
    {
    public:
        void set_int_param(const std::string& parm_name, int val) override;
        void set_int_param(const std::string& parm_name, const std::string& expr) override;
        void set_str_param(const std::string& parm_name, const std::string& str) override;
        void set_lit_param(const std::string& parm_name, const std::string& str) override;

        genie::Port* create_clock_port(const std::string& name, genie::Port::Dir dir, 
            const std::string& hdl_sig);
        genie::Port* create_reset_port(const std::string& name, genie::Port::Dir dir,
            const std::string& hdl_sig);
        genie::PortConduit* create_conduit_port(const std::string& name, genie::Port::Dir dir);
        genie::PortRS* create_rs_port(const std::string& name, genie::Port::Dir dir,
            const std::string& clk_port_name);

    public:
		using Links = std::vector<Link*>;
		using Params = std::unordered_map<std::string, NodeParam*>;

        // Internal API
		virtual ~Node();
		virtual Node* instantiate() = 0;

        const std::string& get_hdl_name() const { return m_hdl_name; }
        Node* get_parent_node() const;
        hdl::HDLState& get_hdl_state() { return m_hdl_state; }

        
        void resolve_params();
        NodeParam* get_param(const std::string& name);
        const Params& get_params() const { return m_params; }
        
		Port* add_port(Port* p);
		template<class P = Port>
		P* get_port(const std::string& path)
		{
			return get_child_as<P>(path);
		}

		Links get_links() const;
		Links get_links(NetType) const;
		Links get_links(HierObject* src, HierObject* sink, NetType net) const;
		Link* connect(HierObject* src, HierObject* sink, NetType net);
		void disconnect(HierObject* src, HierObject* sink, NetType net);
		void disconnect(Link*);
		Link* splice(Link* orig, HierObject* new_sink, HierObject* new_src);

    protected:
		Node(const std::string& name, const std::string& hdl_name);
		Node(const Node&);

		void set_param(const std::string& name, NodeParam* param);

        std::string m_hdl_name;
		Params m_params;
        hdl::HDLState m_hdl_state;
		std::unordered_map<NetType, Links> m_links;
    };
}
}