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

		int get_n_outputs() const;
		TopoPort* get_topo_input() const;
		TopoPort* get_topo_output() const;		
		RVDPort* get_rvd_input() const;
		RVDPort* get_rvd_output(int idx) const;
        void set_uses_bp(bool);

		void refine(NetType) override;
		HierObject* instantiate() override;
		void do_post_carriage() override;
		void configure();

		Port* locate_port(Dir dir, NetType type) override;

	protected:
		void init_vlog();

		CarrierProtocol m_proto;
        bool m_uses_bp;
	};
}