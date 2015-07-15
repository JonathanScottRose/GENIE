#pragma once

#include "genie/structure.h"
#include "genie/protocol.h"

namespace genie
{
	class RVDPort;
	class TopoPort;

	class NodeReg : public Node
	{
	public:
		NodeReg();

		RVDPort* get_input() const;
		RVDPort* get_output() const;
		TopoPort* get_topo_input() const;
		TopoPort* get_topo_output() const;

		void refine(NetType type) override;
		HierObject* instantiate() override;
		void do_post_carriage() override;
		Port* locate_port(Dir dir, NetType type) override;

	protected:
		CarrierProtocol m_proto;
	};
}