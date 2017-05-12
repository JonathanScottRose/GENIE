#pragma once

#include "node.h"
#include "address.h"
#include "protocol.h"

namespace genie
{
namespace impl
{
	class PortClock;

	class NodeClockX : public Node, public ProtocolCarrier
	{
	public:
		static void init();

		NodeClockX();

		NodeClockX* clone() const override;
		void prepare_for_hdl() override;

		PortRS* get_indata_port() const;
		PortRS* get_outdata_port() const;
		PortClock* get_inclock_port() const;
		PortClock* get_outclock_port() const;

	protected:
		NodeClockX(const NodeClockX&) = default;

		void init_vlog();
	};
}
}