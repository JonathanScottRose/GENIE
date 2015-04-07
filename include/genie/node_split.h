#pragma once

#include "genie/structure.h"
#include "genie/net_topo.h"
#include "genie/net_rvd.h"

namespace genie
{
	class NodeSplit : public Node
	{
	public:
		static void init();

		NodeSplit();
		~NodeSplit();

		void refine(NetType) override;

		TopoPort* get_topo_input() const;
		TopoPort* get_topo_output() const;

		int get_n_outputs() const;
		RVDPort* get_rvd_input() const;
		RVDPort* get_rvd_output(int idx) const;
	};
}