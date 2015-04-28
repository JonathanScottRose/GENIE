#pragma once

#include "genie/structure.h"
#include "net_clock.h"
#include "net_rvd.h"
#include "net_reset.h"

namespace genie
{
	class NodeClockX : public Node
	{
	public:
		NodeClockX(bool use_backpressure = true);
		~NodeClockX();

		ResetPort* get_reset_port() const;
		RVDPort* get_indata_port() const;
		RVDPort* get_outdata_port() const;
		ClockPort* get_inclock_port() const;
		ClockPort* get_outclock_port() const;
		
		bool uses_backpressure() const;

		HierObject* instantiate() override;
		void do_post_carriage() override;

	protected:
		bool m_use_backpressure;
		CarrierProtocol m_proto;
	};
}