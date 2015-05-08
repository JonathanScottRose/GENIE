#pragma once

#include "genie/common.h"

namespace genie
{
	class APortIndex : public Aspect
	{
	public:
		PROP_GET_SET(index, int, index);
		int index;
	};
}