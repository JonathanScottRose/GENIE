#include "instance_node.h"

using namespace ct;
using namespace ct::P2P;


DataPort* InstanceNode::create_data_port(Port::Dir dir, Spec::DataInterface* iface, Spec::Instance* inst)
{
	DataPort* result = new DataPort(this, dir);

	// Assign clock
	ClockResetPort* clockport = (ClockResetPort*)this->get_port(iface->get_clock());
	assert(clockport);
	result->set_clock(clockport);

	// Create data port protocol
	Protocol proto;

	// Add fields
	for (auto& i : iface->signals())
	{
		int width = i->get_width().get_value([=] (const std::string& name)
		{
			return &(inst->get_param_binding(name));
		});

		std::string field_name = i->get_field_name();
		Field::Sense field_sense = field_name == "ready" ? Field::Sense::REV :
			Field::Sense::FWD;

		Field* f = new Field(field_name, width, field_sense);
		proto.add_field(f);
	}

	result->set_proto(proto);

	return result;
}

InstanceNode::InstanceNode(Spec::Instance* def)
: Node(Node::INSTANCE), m_instance(def)
{
	m_name = def->get_name();
	
	Spec::Component* comp = Spec::get_component(def->get_component());

	// Create ports for interfaces
	// Do clock/reset ones first (reverse order cause splice works that way)
	Spec::Component::InterfaceList ifaces;
	ifaces.splice_after(ifaces.before_begin(), comp->get_interfaces(Spec::Interface::SEND));
	ifaces.splice_after(ifaces.before_begin(), comp->get_interfaces(Spec::Interface::RECV));
	ifaces.splice_after(ifaces.before_begin(), comp->get_interfaces(Spec::Interface::CLOCK_SRC));
	ifaces.splice_after(ifaces.before_begin(), comp->get_interfaces(Spec::Interface::CLOCK_SINK));
	ifaces.splice_after(ifaces.before_begin(), comp->get_interfaces(Spec::Interface::RESET_SRC));
	ifaces.splice_after(ifaces.before_begin(), comp->get_interfaces(Spec::Interface::RESET_SINK));

	for (Spec::Interface* ifacedef : ifaces)
	{
		Port::Dir pdir = Port::OUT;
		Port* port = nullptr;

		switch (ifacedef->get_type())
		{
		case Spec::Interface::CLOCK_SINK: pdir = Port::IN;
		case Spec::Interface::CLOCK_SRC:
			port = new ClockResetPort(Port::CLOCK, pdir, this);
			break;
		case Spec::Interface::RESET_SINK: pdir = Port::IN;
		case Spec::Interface::RESET_SRC:
			port = new ClockResetPort(Port::RESET, pdir, this);
			break;
		case Spec::Interface::RECV: pdir = Port::IN;
		case Spec::Interface::SEND:
			port = create_data_port(pdir, (Spec::DataInterface*)ifacedef, def);
			break;
		default:
			assert(false);
		}

		port->set_name(ifacedef->get_name());
		port->set_iface_def(ifacedef);
		add_port(port);
	}
}

InstanceNode::~InstanceNode()
{
}
