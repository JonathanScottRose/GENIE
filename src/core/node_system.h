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
		class NodeFlowState;
	}

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

    public:
        NodeSystem(const std::string& name);
		~NodeSystem();

		NodeSystem* clone() const override;
        NodeSystem* instantiate() const override;
        
		PROP_GET_SET(flow_state, flow::NodeFlowState*, m_flow_state);
        std::vector<Node*> get_nodes() const;

		NodeSystem* create_snapshot(unsigned dom_id);
		void reintegrate_snapshot(NodeSystem* src);

    protected:
		NodeSystem(const NodeSystem&);

		flow::NodeFlowState* m_flow_state;
    };
}
}