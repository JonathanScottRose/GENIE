#pragma once

#include "genie/structure.h"
#include "genie/graph.h"

namespace genie
{
namespace flow
{
// Convert a network into a Graph
    using N2GRemapFunc = std::function<Port*(Port*)>;

    graphs::Graph net_to_graph(System* sys, NetType ntype, bool include_internal,
        graphs::VAttr<Port*>* v_to_obj,
        graphs::RVAttr<Port*>* obj_to_v,
        graphs::EAttr<Link*>* e_to_link,
        graphs::REAttr<Link*>* link_to_e,
        const N2GRemapFunc& remap = N2GRemapFunc());

    void apply_latency_constraints(System* sys);
}
}
