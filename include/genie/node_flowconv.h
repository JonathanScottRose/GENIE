#pragma once

#include "genie/structure.h"

namespace genie
{
	class NodeFlowConv : public Node
	{
	public:
		NodeFlowConv(bool to_flow);

        bool is_interconnect() const override { return true; }

		void configure();
		RVDPort* get_input() const;
		RVDPort* get_output() const;

		HierObject* instantiate() override;
		void do_post_carriage() override;

        AreaMetrics get_area_usage() const override;

	protected:
        int m_in_width;
        int m_out_width;

		bool m_to_flow;
		CarrierProtocol m_proto;
	};
}