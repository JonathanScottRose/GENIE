#include "pch.h"
#include "hdl_elab.h"
#include "hdl.h"
#include "node_system.h"
#include "port_clockreset.h"
#include "port_conduit.h"
#include "port_rs.h"
#include "net_clockreset.h"
#include "net_conduit.h"
#include "net_rs.h"

using namespace genie::impl;

namespace
{
	template<class PORTTYPE>
	void do_clockreset(NodeSystem* sys, NetType nettype)
	{
		auto links = sys->get_links(nettype);

		for (auto link : links)
		{
			auto src = dynamic_cast<PORTTYPE*>(link->get_src());
			auto sink = dynamic_cast<PORTTYPE*>(link->get_sink());

			sys->get_hdl_state().connect(src->get_binding(), sink->get_binding());
		}
	}
}

void hdl::elab_system(NodeSystem* sys)
{
	do_clockreset<PortClock>(sys, NET_CLOCK);
	do_clockreset<PortReset>(sys, NET_RESET);
}