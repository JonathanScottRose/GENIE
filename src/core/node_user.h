#pragma once

#include "node.h"

namespace genie
{
namespace impl
{
    class NodeUser : virtual public genie::Module, public Node, public IInstantiable
    {
    public:
		genie::Link* create_internal_link(genie::PortRS* src, genie::PortRS* sink,
			unsigned latency) override;

    public:
        NodeUser(const std::string& name, const std::string& hdl_name);

		NodeUser* clone() const override;
        HierObject* instantiate() const override;
		void prepare_for_hdl() override;
		void annotate_timing() override;
		AreaMetrics annotate_area() override;

    protected:
        NodeUser(const NodeUser&);
    };
}
}