#pragma once

#include "ct/common.h"
#include "ct/networks.h"
#include "ct/structure.h"
#include "ct/connections.h"

//
// Built-in fundamental Reset network type
//

namespace ct
{
	// Network info
	class NetReset : public NetworkDef
	{
	public:
		static const NetworkID ID;
		NetReset();
	};

	// Port template
	class ResetPortDef : public PortDef
	{
	public:
		ResetPortDef(const std::string& name, Dir dir);
		~ResetPortDef();

		Port* instantiate();
	};

	// Port instance/state
	class ResetPort : public Port, public Endpoint
	{
	public:
		ResetPort(const std::string& name, ResetPortDef* def);
		~ResetPort();

		virtual Endpoint* conn_get_default_ep();
		virtual Dir conn_get_dir() const;
		virtual const NetworkID& ep_get_net_type() const;
	};
}