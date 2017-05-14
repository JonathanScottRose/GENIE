#pragma once

#include "hierarchy.h"
#include "hdl.h"
#include "genie/node.h"

namespace genie
{
namespace impl
{
	class IInstantiable;
	class NodeParam;
    class ParamResolver;
	class Link;
	class Port;

    class Node : virtual public genie::Node, public HierObject
    {
    public:
        void set_int_param(const std::string& parm_name, int val) override;
        void set_int_param(const std::string& parm_name, const std::string& expr) override;
        void set_str_param(const std::string& parm_name, const std::string& str) override;
        void set_lit_param(const std::string& parm_name, const std::string& str) override;

		genie::Port* create_clock_port(const std::string& name, genie::Port::Dir dir,
			const std::string& hdl_sig) override;
		genie::Port* create_reset_port(const std::string& name, genie::Port::Dir dir,
			const std::string& hdl_sig) override;
		genie::PortConduit* create_conduit_port(const std::string& name, 
			genie::Port::Dir dir) override;
		genie::PortRS* create_rs_port(const std::string& name, genie::Port::Dir dir,
			const std::string& clk_port_name) override;

    public:
		using Links = std::vector<Link*>;
		using Params = std::unordered_map<std::string, NodeParam*>;

        // Internal API
		virtual ~Node();

		virtual void prepare_for_hdl() = 0;

        const std::string& get_hdl_name() const { return m_hdl_name; }
        Node* get_parent_node() const;
        hdl::HDLState& get_hdl_state() { return m_hdl_state; }

        void resolve_params();
        NodeParam* get_param(const std::string& name);
        const Params& get_params() const { return m_params; }
		void set_bits_param(const std::string parm_name, const BitsVal& val);

		Port* add_port(Port* p);
		template<class P = Port>
		P* get_port(const std::string& path) const
		{
			return get_child_as<P>(path);
		}

		Links get_links() const;
		const Links& get_links(NetType);
		Links get_links(HierObject* src, HierObject* sink, NetType net) const;
		Link* get_link(LinkID);
		Link* connect(HierObject* src, HierObject* sink, NetType net);
		void disconnect(HierObject* src, HierObject* sink, NetType net);
		void disconnect(Link*);
		Link* splice(Link* orig, HierObject* new_sink, HierObject* new_src);
		bool is_link_internal(Link*) const;

		PROP_GET_SET(link_relations, LinkRelations*, m_link_rel);

    protected:
		Node(const std::string& name, const std::string& hdl_name);
		Node(const Node&);

		void set_param(const std::string& name, NodeParam* param);
		void copy_links(const Node& src, const Links* just_these = nullptr);
		void copy_link_rel(const Node& src);

		LinkID add_link(NetType, Link*);
		LinksContainer& get_links_cont(NetType);

        std::string m_hdl_name;
		Params m_params;
        hdl::HDLState m_hdl_state;
		std::vector<LinksContainer> m_links;
		LinkRelations* m_link_rel;
    };

	class IInstantiable
	{
	public:
		virtual Node* instantiate() const = 0;

	protected:
		~IInstantiable() = default;
	};
}
}