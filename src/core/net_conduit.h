#pragma once

#include "network.h"

namespace genie
{
namespace impl
{
	class NetConduit;
	using LinkConduit = Link;

	extern NetType NET_CONDUIT;

	class NetConduit : public NetworkDef
	{
	public:
		static void init();
		Link* create_link() const override;

	protected:
		NetConduit();
		~NetConduit() = default;
	};
}
}