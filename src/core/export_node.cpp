#include "ct.h"
#include "export_node.h"

using namespace ct;
using namespace ct::Core;

ExportNode::ExportNode(System* sys)
: m_sys(sys)
{
	set_type(get_registry()->get_nodetype("export"));
	set_name(sys->get_name());
}

ExportNode::~ExportNode()
{
}

