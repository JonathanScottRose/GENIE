#pragma once

#include "hierarchy.h"
#include "hdl.h"
#include "network.h"
#include "genie/node.h"
#include "prim_db.h"

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

		void reintegrate(HierObject*) override;
		void reintegrate_partial(Node* src,
			const std::vector<HierObject*>& objs,
			const std::vector<Link*>& links);

		virtual void prepare_for_hdl() = 0;
		virtual void annotate_timing() = 0;
		virtual AreaMetrics annotate_area() = 0;

		PROP_GET_SET(hdl_name, const std::string&, m_hdl_name);
        Node* get_parent_node() const;
		PROP_GET_SETR(hdl_state, hdl::HDLState&, m_hdl_state);

        void resolve_size_params();
		void resolve_all_params();
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
		template<class T=Link>
		std::vector<T*> get_links_casted(NetType net)
		{
			return get_links_cont(net).get_all_casted<T>();
		}
		Links get_links(NetType);
		Links get_links(HierObject* src, HierObject* sink, NetType net) const;
		Link* get_link(LinkID);
		Link* connect(HierObject* src, HierObject* sink, NetType net);
		void disconnect(HierObject* src, HierObject* sink, NetType net);
		void disconnect(Link*);
		Link* splice(Link* orig, HierObject* new_sink, HierObject* new_src);
		bool is_link_internal(Link*) const;
		
		void copy_links_from(const Node& src, const Links& links);

		PROP_GETR(link_relations, LinkRelations&, m_link_rel);

    protected:
		Node(const std::string& name, const std::string& hdl_name);
		Node(const Node&, bool copy_contents = true);

		void set_param(const std::string& name, NodeParam* param);

		LinkID add_link(NetType type, Link*);
		Link* remove_link(LinkID);
		LinksContainer& get_links_cont(NetType);

        std::string m_hdl_name;
		Params m_params;
        hdl::HDLState m_hdl_state;
		std::vector<LinksContainer> m_links;
		LinkRelations m_link_rel;
    };
}
}