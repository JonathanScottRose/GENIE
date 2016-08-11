#pragma once

#include "genie/structure.h"
#include "genie/net_rvd.h"
#include "genie/net_topo.h"
#include "genie/protocol.h"

namespace genie
{
	class NodeMerge : public Node
	{
	public:
		static const FieldID FIELD_EOP;

		NodeMerge();
		~NodeMerge();

        bool is_interconnect() const override { return true; }

		int get_n_inputs() const;
		TopoPort* get_topo_input() const;
		TopoPort* get_topo_output() const;
		RVDPort* get_rvd_input(int i) const;
		RVDPort* get_rvd_output() const;

		void refine(NetType) override;
		HierObject* instantiate() override;
		void do_post_carriage() override;

        void configure();
		void do_exclusion_check();

		Port* locate_port(Dir dir, NetType type) override;

        AreaMetrics get_area_usage() const override;

	protected:
		void init_vlog();

        bool m_is_exclusive;
		CarrierProtocol m_proto;
	};
}