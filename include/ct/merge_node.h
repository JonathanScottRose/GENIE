#pragma once

#include "ct/p2p.h"
#include "ct/common.h"

namespace ct
{
namespace P2P
{
	class MergeNode : public Node
	{
	public:
		MergeNode(const std::string& name, int n_inputs);

		PROP_GET(n_inputs, int, m_n_inputs);

		Port* get_inport(int i);
		Port* get_outport();
		ClockResetPort* get_clock_port();
		ClockResetPort* get_reset_port();
		Port* get_free_inport();
		Protocol& get_proto();

		void register_flow(Flow* flow, Port* port);
		int get_inport_idx(Port* port);
		PortList trace(Port* in, Flow* f);
		Port* rtrace(Port* port, Flow* flow);
		void carry_fields(const FieldSet& set);
		void configure_2();

	protected:
		int m_n_inputs;
	};
}
}