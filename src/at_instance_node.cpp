#include "at_instance_node.h"
#include "at_manager.h"
#include "at_spec.h"
#include "ati_channel.h"
#include "ati_sig_pack.h"

ATInstanceNode::ATInstanceNode(ATInstanceDef* def)
: m_instance(def)
{
	m_type = ATNetNode::INSTANCE;

	ATManager* mgr = ATManager::inst();
	ATSpec* spec = mgr->get_spec();
	ATComponentDef* compdef = spec->get_component_def(def->comp_name);
	assert(compdef);

	// Create ports for interfaces
	for (auto i = compdef->iface_begin(); i != compdef->iface_end(); ++i)
	{
		ATInterfaceDef* ifacedef = i->second;
		ATLinkProtocol proto;

		proto.addr_width = ifacedef->has_address() ?
			ifacedef->get_address_signal()->width : 0;

		proto.data_width = 0;
		if (ifacedef->has_data()) proto.data_width += ifacedef->get_data_width();
		if (ifacedef->has_header()) proto.data_width += ifacedef->get_header_width();

		proto.has_valid = ifacedef->has_valid();
		proto.has_ready = ifacedef->has_ready();
		proto.is_packet = ifacedef->has_sop();
		assert(ifacedef->has_sop() == ifacedef->has_eop());
		
		switch(ifacedef->get_type())
		{
		case ATInterfaceDef::SEND:
			{
				ATNetOutPort* port = new ATNetOutPort(this);
				port->set_proto(proto);
				port->set_clock(ifacedef->get_clock());
				m_outports.push_back(port);
				m_send_map[ifacedef->get_name()] = port;
			}
			break;

		case ATInterfaceDef::RECV:
			{
				ATNetInPort* port = new ATNetInPort(this);
				port->set_proto(proto);
				port->set_clock(ifacedef->get_clock());
				m_inports.push_back(port);
				m_recv_map[ifacedef->get_name()] = port;
			}
			break;

		default:
			assert(false);
		}
	}
}


ATInstanceNode::~ATInstanceNode()
{
}


ATNetInPort* ATInstanceNode::get_port_for_recv(const std::string& name)
{
	return m_recv_map[name];
}


ATNetOutPort* ATInstanceNode::get_port_for_send(const std::string &name)
{
	return m_send_map[name];
}


void ATInstanceNode::instantiate()
{
	ATManager* mgr = ATManager::inst();
	ATSpec* spec = mgr->get_spec();

	std::string& compname = m_instance->comp_name;
	std::string& instname = m_instance->inst_name;

	ATComponentDef* compdef = spec->get_component_def(compname);
	assert(compdef);

	sc_module* mod = (compdef->get_inster())(instname.c_str());
	m_impl = mod;

	for (auto i = compdef->clock_begin(); i != compdef->clock_end(); ++i)
	{
		const std::string& name = i->first;
		sc_clock* sig = mgr->get_clock(name);
		assert(sig);

		i->second->binder(*mod, sig);
	}

	// Create packers/unpackers, connect
	for (auto i = m_recv_map.begin(); i != m_recv_map.end(); ++i)
	{
		ATNetInPort* port = i->second;
		ATInterfaceDef* ifacedef = compdef->get_iface(i->first);
		assert(ifacedef);

		// Create unpacker
		std::string name(instname);
		name.append("_unpack_");
		name.append(ifacedef->get_name());
		ati_signal_unpacker* pack = new ati_signal_unpacker(port->get_proto(), name.c_str());

		// Tell the netports about the implementation
		port->set_impl(&pack->recv());

		// Register signals with the unpacker
		int idx = 0;

		if (ifacedef->has_data())
		{
			for (auto i = ifacedef->data_sigs_begin(); i != ifacedef->data_sigs_end(); ++i)
			{
				ATDataSignalDef* sigdef = i->second;
				sc_bv_signal* sig = sigdef->creator();
				pack->add_data_signal(sig, idx, sigdef->width);
				sigdef->binder(*mod, sig);

				idx += sigdef->width;
			}
		}

		if (ifacedef->has_header())
		{
			for (auto i = ifacedef->header_sigs_begin(); i != ifacedef->header_sigs_end(); ++i)
			{
				ATDataSignalDef* sigdef = i->second;
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

	for (auto i : m_send_map)
	{
		ATNetOutPort* port = i.second;
		ATInterfaceDef* ifacedef = compdef->get_iface(i.first);
		assert(ifacedef);

		// Create packer
		std::string name(instname);
		name.append("_pack_");
		name.append(ifacedef->get_name());
		ati_signal_packer* pack = new ati_signal_packer(port->get_proto(), name.c_str());

		// Tell the netports about the implementation
		port->set_impl(&pack->send());

		// Register signals with the packer
		int idx = 0;

		if (ifacedef->has_data())
		{
			for (ATInterfaceDef::DataSigMap::iterator i = ifacedef->data_sigs_begin();
				i != ifacedef->data_sigs_end(); ++i)
			{
				ATDataSignalDef* sigdef = i->second;
				sc_bv_signal* sig = sigdef->creator();
				pack->add_data_signal(sig, idx, sigdef->width);
				sigdef->binder(*mod, sig);

				idx += sigdef->width;
			}
		}

		if (ifacedef->has_header())
		{
			for (ATInterfaceDef::DataSigMap::iterator i = ifacedef->header_sigs_begin();
				i != ifacedef->header_sigs_end(); ++i)
			{
				ATDataSignalDef* sigdef = i->second;
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