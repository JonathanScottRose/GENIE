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

	class LinkTopo : virtual public genie::LinkTopo, public Link
	{
	public:
		virtual void set_min_regs(unsigned) override;

	public:
		LinkTopo();
		LinkTopo(const LinkTopo&) = default;
		~LinkTopo();

		Link* clone() const override;

		PROP_GET(min_regs, unsigned, m_min_regs);

	protected:
		unsigned m_min_regs;
	};
}
}