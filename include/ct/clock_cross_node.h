#pragma once

#include "ct/p2p.h"

namespace ct
{
	namespace P2P
	{
		class ClockCrossNode : public Node
		{
		public:
			ClockCrossNode(const std::string& name);

			ClockResetPort* get_reset_port();

			ClockResetPort* get_inclock_port();
			ClockResetPort* get_outclock_port();
			
			Port* get_inport();
			Port* get_outport();

			void configure_1();
			void configure_2();
			PortList trace(Port* port, Flow* flow);
			Port* rtrace(Port* port, Flow* flow);
			void carry_fields(const FieldSet& set);
		};
	}
}
