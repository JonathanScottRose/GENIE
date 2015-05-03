#pragma once

#include "genie/networks.h"
#include "genie/structure.h"

namespace genie
{
	extern const NetType NET_RESET;

	class ResetPort : public Port
	{
	public:
		static const SigRoleID ROLE_RESET;

		ResetPort(Dir dir);
		ResetPort(Dir dir, const std::string& name);
		~ResetPort();

		HierObject* instantiate() override;
	};
}