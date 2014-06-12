#include "ct/export_node.h"

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

		// attach clock assignment too
		if (other_port->get_type() == Port::DATA)
		{
			DataPort* other_port_d = (DataPort*)other_port;
			ClockResetPort* other_clock_port = other_port_d->get_clock();
			ClockResetPort* clock_src = (ClockResetPort*)other_clock_port->get_first_connected_port();

			// Make sure that the exported port gets its clock signal from the export node
			assert(clock_src->get_type() == Port::CLOCK &&
				clock_src->get_dir() == Port::OUT &&
				clock_src->get_parent() == this);

			DataPort* exp_port_d = (DataPort*)exp_port;
			exp_port_d->set_clock(clock_src);
		}
	}
}



