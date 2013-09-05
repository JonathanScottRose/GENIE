#include "instance_node.h"

using namespace ct;
using namespace ct::P2P;

namespace
{
	DataPort* create_data_port(Node* node, Port::Dir dir, Spec::DataInterface* iface)
	{
		DataPort* result = new DataPort(node, dir);

		// Assign clock
		ClockResetPort* clockport = (ClockResetPort*)node->get_port(iface->get_clock());
		assert(clockport);
		result->set_clock(clockport);

		// Create protocol
		Protocol proto;

		// Add fields
		for (auto& i : iface->signals())
		{
			Protocol::Field* f = nullptr;

			switch (i->get_type())
			{
			case Spec::Signal::VALID:
				f = new Protocol::Field("valid", 1, Protocol::Field::FWD, i);
				break;
			case Spec::Signal::READY:
				f = new Protocol::Field("ready", 1, Protocol::Field::REV, i);
				break;
			case Spec::Signal::DATA:
				{
					std::string nm = "data";
					if (!i->get_subtype().empty())
						nm += '.' + i->get_subtype();
					f = new Protocol::Field(nm, i->get_width(), Protocol::Field::FWD, i);
				}
				break;
			case Spec::Signal::HEADER:
				{
					std::string nm = "header";
					if (!i->get_subtype().empty())
						nm += '.' + i->get_subtype();
					f = new Protocol::Field(nm, i->get_width(), Protocol::Field::FWD, i);
				}
				break;
			case Spec::Signal::SOP:
				f = new Protocol::Field("sop", 1, Protocol::Field::FWD, i);
				break;
			case Spec::Signal::EOP:
				f = new Protocol::Field("eop", 1, Protocol::Field::FWD, i);
				break;
			case Spec::Signal::LINK_ID:
				f = new Protocol::Field("link_id", 1, Protocol::Field::FWD, i);
				break;
			case Spec::Signal::LP_ID:
				f = new Protocol::Field("lp_id", 1, Protocol::Field::FWD, i);
			default:
				assert(false);
			}

			proto.add_field(f);
		}

		result->set_proto(proto);

		return result;
	}

	ClockResetPort* create_cr_port(Node* node, Port::Type type, Port::Dir dir,
		Spec::ClockResetInterface* iface)
	{
		Spec::Signal* sigdef = iface->signals().front();
		ClockResetPort* result = new ClockResetPort(type, dir, node, sigdef);

		return result;
	}
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
			port = create_cr_port(this, Port::CLOCK, pdir, (Spec::ClockResetInterface*)ifacedef);
			break;
		case Spec::Interface::RESET_SINK: pdir = Port::IN;
		case Spec::Interface::RESET_SRC:
			port = create_cr_port(this, Port::RESET, pdir, (Spec::ClockResetInterface*)ifacedef);
			break;
		case Spec::Interface::RECV: pdir = Port::IN;
		case Spec::Interface::SEND:
			port = create_data_port(this, pdir, (Spec::DataInterface*)ifacedef);
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


/*
void InstanceNode::instantiate()
{
	ATManager* mgr = ATManager::inst();
	ATSpec* spec = mgr->get_spec();

	std::string& compname = m_instance->comp_name;
	std::string& instname = m_instance->inst_name;

	ATComponentDef* compdef = spec->get_component_def(compname);
	assert(compdef);

	sc_module* mod = (compdef->get_inster())(instname.c_str());
	m_impl = mod;

	for (auto it : compdef->clocks())
	{
		const std::string& name = it.first;
		sc_clock* sig = mgr->get_clock(name);
		assert(sig);

		it.second->binder(*mod, sig);
	}

	struct Foo
	{
		ati_signal_punpack_base* punpack;
		ATInterfaceDef* ifacedef;
		ATNetPort* port;
	};

	std::vector<Foo> foos;

	// Create packers/unpackers, connect
	for (auto it : m_inports)
	{
		ATNetInPort* inport = it.second;
		ATInterfaceDef* ifacedef = compdef->get_iface(it.first);
		assert(ifacedef);

		// Create unpacker
		std::string name(instname);
		name.append("_unpack_");
		name.append(ifacedef->get_name());
		ati_signal_unpacker* pack = new ati_signal_unpacker(inport->get_proto(), name.c_str());

		for (ATNetFlow* flow : inport->flows())
		{
			int flow_id = flow->get_id();

			ATLinkDef* link_def = flow->get_def();
			ATLinkPointDef& dest_lp_def = link_def->dest;
			ATEndpointDef* dest_ep_def = spec->get_endpoint_def_for_linkpoint(dest_lp_def);

			int ep_id = dest_ep_def->ep_id;
			int addr = link_def->dest_bind_pos;

			pack->define_flow_mapping(flow_id, ep_id, addr);
		}

		// Tell the netports about the implementation
		inport->set_impl(&pack->recv());

		Foo newfoo = {pack, ifacedef, inport};
		foos.push_back(newfoo);
	}

	for (auto it : m_outports)
	{
		ATNetOutPort* outport = it.second;
		ATInterfaceDef* ifacedef = compdef->get_iface(it.first);
		assert(ifacedef);

		// Create packer
		std::string name(instname);
		name.append("_pack_");
		name.append(ifacedef->get_name());
		ati_signal_packer* pack = new ati_signal_packer(outport->get_proto(), name.c_str());

		for (ATNetFlow* flow : outport->flows())
		{
			int flow_id = flow->get_id();

			ATLinkDef* link_def = flow->get_def();
			ATLinkPointDef& src_lp_def = link_def->src;
			ATEndpointDef* src_ep_def = spec->get_endpoint_def_for_linkpoint(src_lp_def);

			int ep_id = src_ep_def->ep_id;
			int addr = ati_signal_punpack_base::ANY_ADDR;
			
			if (src_ep_def->type == ATEndpointDef::UNICAST)
			{
				addr = link_def->src_bind_pos;
			}

			pack->define_flow_mapping(flow_id, ep_id, addr);
		}

		// Tell the netports about the implementation
		outport->set_impl(&pack->send());

		Foo newfoo = {pack, ifacedef, outport};
		foos.push_back(newfoo);
	}

	// Register signals 
	for (Foo& foo : foos)
	{
		ATInterfaceDef* ifacedef = foo.ifacedef;
		ati_signal_punpack_base* pack = foo.punpack;
		ATNetPort* port = foo.port;
		
		int idx = 0;

		if (ifacedef->has_data())
		{
			for (auto it : ifacedef->data_signals())
			{
				ATDataSignalDef* sigdef = it.second;
				sc_bv_signal* sig = sigdef->creator();
				pack->add_data_signal(sig, idx, sigdef->width);
				sigdef->binder(*mod, sig);

				idx += sigdef->width;
			}
		}

		if (ifacedef->has_header())
		{
			for (auto it : ifacedef->header_signals())
			{
				ATDataSignalDef* sigdef = it.second;
				sc_bv_signal* sig = sigdef->creator();
				pack->add_data_signal(sig, idx, sigdef->width);
				sigdef->binder(*mod, sig);

				idx += sigdef->width;
			}
		}

		if (ifacedef->has_address())
		{
			ATAddrSignalDef* sigdef = ifacedef->get_address_signal();
			sc_bv_signal* sig = sigdef->creator();
			pack->add_addr_signal(sig);
			sigdef->binder(*mod, sig);
		}

		if (ifacedef->has_ep())
		{
			ATAddrSignalDef* sigdef = ifacedef->get_ep_signal();
			sc_bv_signal* sig = sigdef->creator();
			pack->add_epid_signal(sig);
			sigdef->binder(*mod, sig);
		}

		if (ifacedef->has_valid())
		{
			ATCtrlSignalDef* sigdef = ifacedef->get_valid_signal();
			sc_signal<bool>* sig = new sc_signal<bool>();
			pack->add_valid_signal(sig);
			sigdef->binder(*mod, sig);
		}

		if (ifacedef->has_ready())
		{
			ATCtrlSignalDef* sigdef = ifacedef->get_ready_signal();
			sc_signal<bool>* sig = new sc_signal<bool>();
			pack->add_ready_signal(sig);
			sigdef->binder(*mod, sig);
		}

		if (ifacedef->has_sop())
		{
			assert(ifacedef->has_eop());

			ATCtrlSignalDef* sigdef = ifacedef->get_sop_signal();
			sc_signal<bool>* sig = new sc_signal<bool>();
			pack->add_sop_signal(sig);
			sigdef->binder(*mod, sig);

			sigdef = ifacedef->get_eop_signal();
			sig = new sc_signal<bool>();
			pack->add_eop_signal(sig);
			sigdef->binder(*mod, sig);
		}
	}
}
*/