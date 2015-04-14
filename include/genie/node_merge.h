#pragma once

#include "genie/structure.h"
#include "genie/net_rvd.h"
#include "genie/net_topo.h"

namespace genie
{
	class NodeMerge : public Node
	{
	public:
		NodeMerge();
		~NodeMerge();

		TopoPort* get_topo_input() const;
		TopoPort* get_topo_output() const;

		int get_n_inputs() const;
		RVDPort* get_rvd_input(int i) const;
		RVDPort* get_rvd_output() const;

		void refine(NetType) override;

	protected:
		bool m_exclusive;
		void init_vlog();
	};
}