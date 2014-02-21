#pragma once

#include "p2p.h"

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