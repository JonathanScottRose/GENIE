#pragma once

#include "core.h"

namespace ct
{
	namespace Core
	{
		class SystemNode : public Node
		{
		public:
			SystemNode();
			SystemNode(System* sys);
			~SystemNode();

			PROP_GET_SET(system, System*, m_sys);

		protected:
			System* m_sys;
		};
	}
}