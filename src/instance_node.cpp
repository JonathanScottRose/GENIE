#include "ct/instance_node.h"

using namespace ct;
using namespace ct::P2P;

InstanceNode::InstanceNode(Spec::Instance* def)
: Node(Node::INSTANCE), m_instance(def)
{
	using namespace ct::Spec;

	m_name = def->get_name();
	
	Component* comp = Spec::get_component(def->get_component());

	// Convert iface definitions into Ports
	for (auto& i : comp->interfaces())
	{
		Interface* ifacedef = i.second;

		// Use this to resolve parameterized widths on the instance's ports
		const Expressions::NameResolver& resolv = [=](const std::string& name)
		{
			return def->get_param_binding(name);
		};

		Port* port = Port::from_interface(ifacedef, resolv);

		add_port(port);
	}

	// Now that all ports exist, go through any of the newly-created DataPorts and associate them
	// with their clock sinks.
	for (auto& i : this->ports())
	{
		auto dport = (DataPort*)i.second;
		if (dport->get_type() != Port::DATA)
			continue;

		auto* diface = (DataInterface*) dport->get_aspect<Interface>("iface_def");

		assert(dport->get_type() == Port::DATA);
		assert(diface->get_type() == Spec::Interface::DATA);

		// Assign clock
		ClockResetPort* clockport = (ClockResetPort*)this->get_port(diface->get_clock());
		assert(clockport);
		dport->set_clock(clockport);
	}
}

