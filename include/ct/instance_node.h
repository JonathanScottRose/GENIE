#pragma once

#include "ct/p2p.h"
#include "ct/spec.h"
#include "ct/common.h"

namespace ct
{
namespace P2P
{
	class InstanceNode : public Node
	{
	public:
		InstanceNode(Spec::Instance* def);
	
		PROP_GET_SET(instance, Spec::Instance*, m_instance);
	
	private:
		void convert_fields(Port* port, Spec::Interface* iface, Spec::Instance* inst);
		void set_clock(Port* port, Spec::Interface* iface);
		Port* create_data_port(Port::Dir dir, Spec::Interface* iface, Spec::Instance* inst);

		Spec::Instance* m_instance;
	};
}
}