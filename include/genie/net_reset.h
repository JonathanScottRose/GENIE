#pragma once

#include "genie/networks.h"
#include "genie/structure.h"

namespace genie
{
	extern NetType NET_RESET;

    class NetReset : public Network
    {
    public:
        static void init();
        NetReset();
        Port* create_port(Dir dir) override;
    };

	class ResetPort : public Port
	{
	public:
		static SigRoleID ROLE_RESET;

		ResetPort(Dir dir);
		ResetPort(Dir dir, const std::string& name);
		~ResetPort();

		HierObject* instantiate() override;
	};
}