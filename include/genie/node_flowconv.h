#pragma once

#include "genie/structure.h"

namespace genie
{
	class NodeFlowConv : public Node
	{
	public:
		NodeFlowConv(bool to_flow);

		void configure();
		RVDPort* get_input() const;
		RVDPort* get_output() const;

		HierObject* instantiate() override;
		void do_post_carriage() override;

	protected:
		bool m_to_flow;
		CarrierProtocol m_proto;
	};
}