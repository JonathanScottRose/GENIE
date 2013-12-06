#pragma once

#include "core.h"

namespace ct
{
namespace Core
{
	class ExportNode : public Node
	{
	public:
		ExportNode(System* sys);
		~ExportNode();

		PROP_GET(system, System*, m_sys);

	protected:
		System* m_sys;
	};
}
}