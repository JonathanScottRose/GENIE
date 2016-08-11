#pragma once

#include "genie/structure.h"
#include "genie/net_topo.h"
#include "genie/net_rvd.h"
#include "genie/protocol.h"

namespace genie
{
	class NodeSplit : public Node
	{
	public:
		static const FieldID FIELD_FLOWID;

		NodeSplit();
		~NodeSplit();

        bool is_interconnect() const override { return true; }

		int get_n_outputs() const;
		TopoPort* get_topo_input() const;
		TopoPort* get_topo_output() const;		
		RVDPort* get_rvd_input() const;
		RVDPort* get_rvd_output(int idx) const;

		void refine(NetType) override;
		HierObject* instantiate() override;
		void do_post_carriage() override;
		void configure();

		Port* locate_port(Dir dir, NetType type) override;

        AreaMetrics get_area_usage() const override;

	protected:
		void init_vlog();

        int m_fid_width;
		CarrierProtocol m_proto;
	};
}