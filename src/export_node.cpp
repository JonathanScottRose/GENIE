#include "export_node.h"

using namespace ct::P2P;

ExportNode::ExportNode(System* sys)
: Node(EXPORT)
{
	set_name(sys->get_name());
}

void ExportNode::configure_1()
{
	// make exports mimic the things they are connected to
	for (auto& i : ports())
	{
		Port* exp_port = i.second;
		Port* other_port = exp_port->get_first_connected_port();

		// attach protocol
		exp_port->set_proto(other_port->get_proto());
	}
}



