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

	class NodeMDelay: public Node, public ProtocolCarrier
	{
	public:
		static void init();
		static AreaMetrics estimate_area(unsigned width, unsigned depth, bool bp);

		// Create a new one
		NodeMDelay();

		// Generic copy of an existing one
		NodeMDelay* clone() const override;
		void prepare_for_hdl() override;
		void annotate_timing() override;
		AreaMetrics annotate_area() override;

		PortRS* get_input() const;
		PortRS* get_output() const;
		PortClock* get_clock_port() const;
		void set_delay(unsigned);
		PROP_GET(delay, unsigned, m_delay);

	protected:
		NodeMDelay(const NodeMDelay&);
		void init_vlog();

		unsigned m_delay;
	};
}
}