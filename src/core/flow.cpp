#include "pch.h"
#include "genie/genie.h"
#include "genie/log.h"
#include "genie_priv.h"
#include "vlog_write.h"
#include "hdl_elab.h"
#include "node_system.h"
#include "net_conduit.h"
#include "port_conduit.h"

using namespace genie::impl;

namespace
{
    void resolve_params(NodeSystem* sys)
    {
        sys->resolve_params();
        
        auto nodes = sys->get_nodes();
        for (auto node : nodes)
        {
            node->resolve_params();
        }
    }

	void connect_conduits(NodeSystem* sys)
	{
		// Find conduit links
		auto links = sys->get_links(NET_CONDUIT);

		for (auto cnd_link : links)
		{
			auto cnd_src = dynamic_cast<PortConduit*>(cnd_link->get_src());
			auto cnd_sink = dynamic_cast<PortConduit*>(cnd_link->get_sink());

			// Get the source's subports
			auto src_subs = cnd_src->get_subports();

			// For each src subport, find the subport on the sink with a matching tag
			for (auto src_sub : src_subs)
			{
				auto& tag = src_sub->get_tag();

				// If one is not found, continue but throw warnings later
				auto sink_sub = cnd_sink->get_subport(tag);
				if (!sink_sub)
				{
					genie::log::warn("connecting %s to %s: %s is missing field '%s'",
						cnd_src->get_hier_path().c_str(),
						cnd_sink->get_hier_path().c_str(),
						cnd_sink->get_hier_path().c_str(),
						tag.c_str());
					continue;
				}

				// Does the sub->sub direction need to be reversed?
				bool swapdir = src_sub->get_effective_dir(sys) == Port::Dir::IN;
				if (swapdir)
					std::swap(src_sub, sink_sub);

				sys->connect(src_sub, sink_sub, NET_CONDUIT_SUB);
			}
		}
	}

    void do_system(NodeSystem* sys)
    {
        resolve_params(sys);
		connect_conduits(sys);
    }
}

void genie::do_flow()
{
    auto systems = get_systems();

    for (auto& sys : systems)
    {
        do_system(sys);
    }

    for (auto& sys : systems)
    {
		hdl::elab_system(sys);
        hdl::write_system(sys);
    }
}
