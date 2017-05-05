#pragma once

#include "network.h"

namespace genie
{
namespace impl
{
	class NetClock;
	class NetReset;
	using LinkClock = Link;
	using LinkReset = Link;

	extern NetType NET_CLOCK;
	extern NetType NET_RESET;

	class NetClock : public NetworkDef
	{
	public:
		static void init();
		Link* create_link() const override;

	protected:
		NetClock();
		~NetClock() = default;
	};

	class NetReset : public NetworkDef
	{
	public:
		static void init();
		Link* create_link() const override;

	protected:
		NetReset();
		~NetReset() = default;
	};
}
}