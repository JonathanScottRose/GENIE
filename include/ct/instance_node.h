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

		Spec::Instance* m_instance;
	};
}
}