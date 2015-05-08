#pragma once

#include "genie/structure.h"
#include "genie/net_rvd.h"

namespace genie
{
	class NodeReg : public Node
	{
	public:
		NodeReg();

		RVDPort* get_input() const;
		RVDPort* get_output() const;

		HierObject* instantiate() override;
		void do_post_carriage() override;

	protected:
		CarrierProtocol m_proto;
	};
}