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
		struct PortAspect : public ImplAspect
		{
			Spec::Interface* iface_def;
		};

		InstanceNode(Spec::Instance* def);
		~InstanceNode();
	
		PROP_GET_SET(instance, Spec::Instance*, m_instance);
	
	private:
		void convert_fields(Port* port, Spec::Interface* iface, Spec::Instance* inst);
		void set_clock(Port* port, Spec::Interface* iface);
		Port* create_data_port(Port::Dir dir, Spec::Interface* iface, Spec::Instance* inst);

		Spec::Instance* m_instance;
	};
}
}