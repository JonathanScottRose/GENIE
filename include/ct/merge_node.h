#pragma once

#include "p2p.h"
#include "common.h"

namespace ct
{
namespace P2P
{
	class MergeNode : public Node
	{
	public:
		MergeNode(const std::string& name, const Protocol& proto, int n_inputs);
		~MergeNode();

		PROP_GET(n_inputs, int, m_n_inputs);
		PROP_GET(proto, const Protocol&, m_proto);

		DataPort* get_inport(int i);
		DataPort* get_outport();
		ClockResetPort* get_clock_port();
		ClockResetPort* get_reset_port();

		void register_flow(Flow* flow, DataPort* port);

	protected:
		int m_n_inputs;
		Protocol m_proto;
	};
}
}