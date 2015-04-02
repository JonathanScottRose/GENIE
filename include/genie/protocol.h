#pragma once

#include "genie/common.h"
#include "genie/sigroles.h"

namespace genie
{
	class PhysField
	{
	public:
		PROP_GET_SET(name, std::string&, m_name);

	protected:
		std::string m_name;
	};

	class Protocol
	{

	};
}