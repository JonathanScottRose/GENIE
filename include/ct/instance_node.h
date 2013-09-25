#pragma once

#include "p2p.h"
#include "spec.h"
#include "common.h"

namespace ct
{
namespace P2P
{
	class InstanceNode : public Node
	{
	public:
		InstanceNode(Spec::Instance* def);
		~InstanceNode();
	
		PROP_GET_SET(instance, Spec::Instance*, m_instance);
	
	private:
		DataPort* create_data_port(Port::Dir dir, Spec::DataInterface* iface, Spec::Instance* inst);

		Spec::Instance* m_instance;
	};
}
}