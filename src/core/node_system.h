#pragma once

#include "node.h"
#include "genie/node.h"

namespace genie
{
namespace impl
{
    class NodeSystem : virtual public Node, virtual public System
    {
    public:
        void create_sys_param(const std::string& name) override;

		genie::Link* create_clock_link(genie::HierObject* src, genie::HierObject* sink) override;
		genie::Link* create_reset_link(genie::HierObject* src, genie::HierObject* sink) override;
		genie::Link* create_conduit_link(genie::HierObject* src, genie::HierObject* sink) override;
		genie::LinkRS* create_rs_link(genie::HierObject* src, genie::HierObject* sink) override;
		genie::Link* create_topo_link(genie::HierObject* src, genie::HierObject* sink) override;

    public:
        NodeSystem(const std::string& name);

        Node* instantiate(const std::string&) override;
        
        std::vector<Node*> get_nodes() const;

    protected:
        NodeSystem(const NodeSystem&, const std::string&);
    };
}
}