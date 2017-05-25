#pragma once

#include "node.h"
#include "network.h"
#include "genie/node.h"

namespace genie
{
namespace impl
{
	namespace flow
	{
		class FlowStateOuter;
	}

	class ExclusivityInfo
	{
	public:
		using Set = std::unordered_set<LinkID>;

		void add(LinkID, LinkID);
		Set& get_set(LinkID);
		bool are_exclusive(LinkID, LinkID);
		
	protected:
		std::unordered_map <LinkID,Set> m_sets;
	};

	using SyncConstraints = std::vector<SyncConstraint>;

    class NodeSystem : virtual public genie::System, public Node, public IInstantiable
    {
    public:
        void create_sys_param(const std::string& name) override;

		genie::Link* create_clock_link(genie::HierObject* src, genie::HierObject* sink) override;
		genie::Link* create_reset_link(genie::HierObject* src, genie::HierObject* sink) override;
		genie::Link* create_conduit_link(genie::HierObject* src, genie::HierObject* sink) override;
		genie::LinkRS* create_rs_link(genie::HierObject* src, genie::HierObject* sink,
			unsigned src_addr, unsigned sink_addr) override;
		genie::LinkTopo* create_topo_link(genie::HierObject* src, genie::HierObject* sink) override;

		genie::Node* create_instance(const std::string& mod_name,
			const std::string& inst_name) override;

		genie::Port* export_port(genie::Port* orig, const std::string& new_name = "") override;

		genie::Node* create_split(const std::string& name = "") override;
		genie::Node* create_merge(const std::string& name = "") override;

		void make_exclusive(const std::vector<genie::LinkRS*>& links) override;
		void add_sync_constraint(const SyncConstraint& constraint) override;
		void set_max_logic_depth(unsigned max_depth);

    public:
        NodeSystem(const std::string& name);
		~NodeSystem();

		NodeSystem* clone() const override;
        Node* instantiate() const override;
		void prepare_for_hdl() override;
        
        std::vector<Node*> get_nodes() const;
		ExclusivityInfo& get_link_exclusivity() const;
		SyncConstraints& get_sync_constraints() const;
		PROP_GET(max_logic_depth, unsigned, m_max_logic_depth);

		NodeSystem* create_snapshot(const std::unordered_set<HierObject*>&, 
			const std::vector<Link*>&);
		void reintegrate_snapshot(NodeSystem* src);

    protected:
		NodeSystem(const NodeSystem&);

		std::shared_ptr<ExclusivityInfo> m_excl_info;
		std::shared_ptr<SyncConstraints> m_sync_constraints;
		unsigned m_max_logic_depth;
    };
}
}