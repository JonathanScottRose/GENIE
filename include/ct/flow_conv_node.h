#pragma once

#include "p2p.h"

namespace ct
{
namespace P2P
{
	class FlowConvNode : public Node
	{
	public:
		FlowConvNode(const std::string& name, bool to_flow, const Protocol& user_proto,
			const DataPort::Flows& flows);
		~FlowConvNode();

		ClockResetPort* get_clock_port();
		ClockResetPort* get_reset_port();
		DataPort* get_inport();
		DataPort* get_outport();

	protected:
	};
}
}
