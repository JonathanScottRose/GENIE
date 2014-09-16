#pragma once

#include "ct/p2p.h"

namespace ct
{
	namespace P2P
	{
		class RegNode : public Node
		{
		public:
			RegNode(const std::string& name);

			ClockResetPort* get_clock_port();
			ClockResetPort* get_reset_port();
			DataPort* get_inport();
			DataPort* get_outport();

			void configure_1();
			void configure_2();
			PortList trace(Port* port, Flow* flow);
			Port* rtrace(Port* port, Flow* flow);
			void carry_fields(const FieldSet& set);
		};
	}
}
