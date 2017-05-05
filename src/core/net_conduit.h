#pragma once

#include "network.h"

namespace genie
{
namespace impl
{
	class NetConduit;
	class NetConduitSub;
	using LinkConduit = Link;
	using LinkConduitSub = Link;

	extern NetType NET_CONDUIT;
	extern NetType NET_CONDUIT_SUB;

	class NetConduit : public NetworkDef
	{
	public:
		static void init();
		Link* create_link() const override;

	protected:
		NetConduit();
		~NetConduit() = default;
	};

	class NetConduitSub : public NetworkDef
	{
	public:
		static void init();
		Link* create_link() const override;

	protected:
		NetConduitSub();
		~NetConduitSub() = default;
	};
}
}