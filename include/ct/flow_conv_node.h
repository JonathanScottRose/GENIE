#pragma once

#include "p2p.h"

namespace ct
{
namespace P2P
{
	class FlowConvNode : public Node
	{
	public:
		FlowConvNode(System* parent, const std::string& name, bool to_flow, const Protocol& user_proto,
			const DataPort::Flows& flows, DataPort* user_port);
		~FlowConvNode();

		ClockResetPort* get_clock_port();
		ClockResetPort* get_reset_port();
		DataPort* get_inport();
		DataPort* get_outport();
		
		PROP_GET_SET(user_port, DataPort*, m_user_port);

		const std::string& get_in_field_name();
		const std::string& get_out_field_name();
		int get_n_entries();
		int get_in_field_width();
		int get_out_field_width();
		const DataPort::Flows& get_flows();
		bool is_to_flow() { return m_to_flow; }

	protected:
		bool m_to_flow;
		DataPort* m_user_port;
	};
}
}
