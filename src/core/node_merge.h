#pragma once

#include "node.h"
#include "protocol.h"

namespace genie
{
namespace impl
{
	class PortRS;

    class NodeMerge : public Node, public ProtocolCarrier
    {
    public:
        static void init();
        
        // Create a new one
		NodeMerge();

        // Generic copy of an existing one
        NodeMerge* clone() const override;
		void prepare_for_hdl() override;
		void annotate_timing() override;
		AreaMetrics annotate_area() override;

		PROP_GET(n_inputs, unsigned, m_n_inputs);
		void create_ports();
		PortRS* get_input(unsigned) const;
		PortRS* get_output() const;

		PROP_GET_SET(exclusive, bool, m_is_exclusive);

    protected:
		NodeMerge(const NodeMerge&) = default;

		void init_vlog();
		void annotate_timing_nonex();
		void annotate_timing_ex();
		AreaMetrics annotate_area_nonex();
		AreaMetrics annotate_area_ex();
		bool does_feed_reg();

		unsigned m_n_inputs;
		bool m_is_exclusive;
    };
}
}