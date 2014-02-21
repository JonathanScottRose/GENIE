#pragma once

#include "p2p.h"

namespace ct
{
namespace P2P
{
	class FlowConvNode : public Node
	{
	public:
		FlowConvNode(const std::string& name, bool to_flow);

		ClockResetPort* get_clock_port();
		ClockResetPort* get_reset_port();
		Port* get_inport();
		Port* get_outport();
		
		PROP_GET_SET(user_port, Port*, m_user_port);

		const std::string& get_in_field_name();
		const std::string& get_out_field_name();
		int get_n_entries();
		int get_in_field_width();
		int get_out_field_width();
		const Port::Flows& get_flows();
		bool is_to_flow() { return m_to_flow; }

		void configure_1();
		void configure_2();
		PortList trace(Port* port, Flow* flow);
		Port* rtrace(Port* port, Flow* flow);
		void carry_fields(const FieldSet& set);

	protected:
		bool m_to_flow;
		Port* m_user_port;
	};
}
}
