#include "instance_node.h"

using namespace ct;
using namespace ct::P2P;


void InstanceNode::convert_fields(Port* port, Spec::Interface* iface, Spec::Instance* inst)
{
	// Create port protocol
	Protocol proto;

	// Add fields
	for (auto& i : iface->signals())
	{
		int width = i->get_width().get_value([=] (const std::string& name)
		{
			return inst->get_param_binding(name);
		});

		std::string field_name = i->get_field_name();
		Field::Sense field_sense = i->get_sense() == Spec::Signal::FWD? Field::FWD : Field::REV;

		Field* f = new Field(field_name, width, field_sense);
		proto.add_field(f);
	}

	port->set_proto(proto);
}

void InstanceNode::set_clock(Port* port, Spec::Interface* iface)
{
	DataPort* dport = (DataPort*) port;
	Spec::DataInterface* diface = (Spec::DataInterface*) iface;

	assert(dport->get_type() == Port::DATA);
	assert(diface->get_type() == Spec::Interface::SEND ||
		diface->get_type() == Spec::Interface::RECV);

	// Assign clock
	ClockResetPort* clockport = (ClockResetPort*)this->get_port(diface->get_clock());
	assert(clockport);
	dport->set_clock(clockport);
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
	ifaces.splice_after(ifaces.before_begin(), comp->get_interfaces(Spec::Interface::CONDUIT));

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
			port = new DataPort(this, pdir);
			convert_fields(port, ifacedef, def);
			set_clock(port, ifacedef);
			break;
		case Spec::Interface::CONDUIT:
			port = new ConduitPort(this);
			convert_fields(port, ifacedef, def);
			break;
		default:
			assert(false);
		}

		port->set_name(ifacedef->get_name());
		
		PortAspect* asp = new PortAspect;
		asp->iface_def = ifacedef;
		port->set_impl("iface_def", asp);

		add_port(port);
	}
}

InstanceNode::~InstanceNode()
{
}
