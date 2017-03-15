#pragma once

#include "network.h"
#include "prop_macros.h"

namespace genie
{
namespace impl
{
	extern NetType NET_INTERNAL;

	class NetInternal : public Network
	{
	public:
		static void init();
		Link* create_link() const override;

	protected:
		NetInternal();
		~NetInternal() = default;
	};

	class LinkInternal : virtual public Link
	{
	public:
	public:
		LinkInternal();
		LinkInternal(const LinkInternal&);
		~LinkInternal();

		Link* clone() const override;

		PROP_GET_SET(latency, unsigned, m_latency);

	protected:
		unsigned m_latency;
	};
}
}