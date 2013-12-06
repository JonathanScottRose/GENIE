#include "ct.h"
#include "core.h"
#include "export_node.h"

using namespace ct;
using namespace ct::Core;

namespace
{
	Registry s_registry;
}

namespace
{
}

Registry* ct::get_registry()
{
	return &s_registry;
}
