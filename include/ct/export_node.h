#pragma once

#include "ct/p2p.h"
#include "ct/spec.h"

namespace ct
{
namespace P2P
{
	class ExportNode : public Node
	{
	public:
		ExportNode(P2P::System* parent, Spec::System* spec);
		void configure_1();
	};
}
}