#include "export_node.h"

using namespace ct::P2P;

ExportNode::ExportNode(System* sys)
: Node(EXPORT)
{
	set_name(sys->get_name());
}

ExportNode::~ExportNode()
{
}

