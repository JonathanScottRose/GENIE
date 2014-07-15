#include "ct/export_node.h"

using namespace ct;
using namespace ct::P2P;

ExportNode::ExportNode(P2P::System* parent, Spec::System* spec)
: Node(EXPORT)
{
	set_name(parent->get_name());

	// Convert Exports into ports
	for (auto& i : spec->objects())
	{
		auto exp = (Spec::Export*)i.second;
		if (exp->get_type() != Spec::SysObject::EXPORT)
			continue;

		Spec::Interface* ifacedef = exp->get_iface();

		// Parameterized port widths are not allowed for exports
		Port* port = Port::from_interface(ifacedef, Expression::get_const_resolver());

		add_port(port);
	}

	// Now that all ports exist, go through any of the newly-created DataPorts and associate them
	// with their clock sinks.
	for (auto& i : this->ports())
	{
		auto dport = (DataPort*)i.second;
		if (dport->get_type() != Port::DATA)
			continue;

		auto* diface = (Spec::DataInterface*)dport->get_aspect<Spec::Interface>("iface_def");

		assert(dport->get_type() == Port::DATA);
		assert(diface->get_type() == Spec::Interface::DATA);

		// Assign clock
		ClockResetPort* clockport = (ClockResetPort*)this->get_port(diface->get_clock());
		assert(clockport);
		dport->set_clock(clockport);
	}
}

void ExportNode::configure_1()
{
	// nothing to configure.
}



