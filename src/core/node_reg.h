#pragma once

#include "node.h"
#include "address.h"
#include "protocol.h"

namespace genie
{
namespace impl
{
	class PortRS;
	class PortClock;

	class NodeReg: public Node, public ProtocolCarrier
	{
	public:
		static void init();

		// Create a new one
		NodeReg();

		// Generic copy of an existing one
		NodeReg* clone() const override;
		void prepare_for_hdl() override;
		void annotate_timing() override;
		AreaMetrics annotate_area() override;

		PortRS* get_input() const;
		PortRS* get_output() const;
		PortClock* get_clock_port() const;

	protected:
		NodeReg(const NodeReg&);

		void init_vlog();
	};
}
}