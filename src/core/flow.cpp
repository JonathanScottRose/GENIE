#include "pch.h"
#include "genie/genie.h"
#include "genie/log.h"
#include "genie_priv.h"
#include "vlog_write.h"
#include "hdl_elab.h"
#include "node_system.h"

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

	

    void do_system(NodeSystem* sys)
    {
        resolve_params(sys);
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
