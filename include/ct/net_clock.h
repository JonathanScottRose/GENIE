#pragma once

#include "ct/common.h"
#include "ct/networks.h"
#include "ct/structure.h"
#include "ct/connections.h"

//
// Built-in fundamental Clock network type
//

namespace ct
{
	// Network info
	class NetClock : public NetworkDef
	{
	public:
		static const NetworkID ID;
		NetClock();
	};

	// Port template
	class ClockPortDef : public PortDef
	{
	public:
		ClockPortDef(const std::string& name, Dir dir);
		~ClockPortDef();

		Port* instantiate();
	};

	// Port instance/state
	class ClockPort : public Port, public Endpoint
	{
	public:
		ClockPort(const std::string& name, ClockPortDef* def);
		~ClockPort();

		virtual Endpoint* conn_get_default_ep();
		virtual Dir conn_get_dir() const;
		virtual const NetworkID& ep_get_net_type() const;
	};
}