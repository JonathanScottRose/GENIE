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
		NodeClockX();
		~NodeClockX();

        bool is_interconnect() const override { return true; }

		RVDPort* get_indata_port() const;
		RVDPort* get_outdata_port() const;
		ClockPort* get_inclock_port() const;
		ClockPort* get_outclock_port() const;

		HierObject* instantiate() override;
		void do_post_carriage() override;

        AreaMetrics get_area_usage() const override;

	protected:
		CarrierProtocol m_proto;
	};
}