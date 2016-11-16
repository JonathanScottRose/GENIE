#pragma once

#include "genie/structure.h"
#include "genie/graph.h"
#include "genie/net_rs.h"

namespace genie
{
namespace flow
{
    void apply_latency_constraints(System* sys);
    void topo_optimize(System* sys);
}
}
