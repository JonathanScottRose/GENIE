#pragma once

#include "network.h"

namespace genie
{
namespace impl
{
	extern NetType NET_TOPO;

	class NetTopo : public NetworkDef
	{
	public:
		static void init();
		Link* create_link() const override;

	protected:
		NetTopo();
		~NetTopo() = default;
	};

	class LinkTopo : public genie::LinkTopo, public Link
	{
	public:
		virtual void set_minmax_regs(unsigned, unsigned) override;

	public:
		LinkTopo();
		LinkTopo(const LinkTopo&) = default;
		~LinkTopo();

		LinkTopo* clone() const override;

		PROP_GET(min_regs, unsigned, m_min_regs);

	protected:
		unsigned m_min_regs;
		unsigned m_max_regs;
	};
}
}