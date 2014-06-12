#pragma once

#include "ct/p2p.h"

namespace ct
{
namespace P2P
{
	class ExportNode : public Node
	{
	public:
		ExportNode(System* sys);
		void configure_1();
	};
}
}