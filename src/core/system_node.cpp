#include "ct.h"
#include "system_node.h"

using namespace ct;
using namespace ct::Core;

SystemNode::SystemNode()
: m_sys(nullptr)
{
}

SystemNode::SystemNode(System* sys)
: m_sys(sys)
{
	set_type(get_registry()->get_nodetype("system"));
	set_name(sys->get_name());
}

SystemNode::~SystemNode()
{
}

