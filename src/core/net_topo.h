#pragma once

#include "network.h"

namespace genie
{
namespace impl
{
	extern NetType NET_TOPO;

	class NetTopo : public Network
	{
	public:
		static void init();
		Link* create_link() const override;

	protected:
		NetTopo();
		~NetTopo() = default;
	};

	class LinkTopo : virtual public Link
	{
	public:
	public:
		LinkTopo();
		LinkTopo(const LinkTopo&);
		~LinkTopo();

		Link* clone() const override;
	protected:
	};
}
}