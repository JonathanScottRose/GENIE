#pragma once

#include "genie/networks.h"
#include "genie/structure.h"

namespace genie
{
	// Globally-accessible network type ID. Maybe move to a static member?
	extern const NetType NET_CONDUIT;

	class ConduitPort : public Port
	{
	public:
		static SigRoleID ROLE_FWD;
		static SigRoleID ROLE_REV;
		static SigRoleID ROLE_IN;
		static SigRoleID ROLE_OUT;
		static SigRoleID ROLE_INOUT;

		ConduitPort(Dir dir);
		ConduitPort(Dir dir, const std::string& name);
		~ConduitPort();

		HierObject* instantiate() override;
	};
}