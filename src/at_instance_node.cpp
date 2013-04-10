#include <algorithm>
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

	m_name = def->inst_name;

	// Create ports for interfaces
	for (auto it : compdef->interfaces())
	{
		ATInterfaceDef* ifacedef = it.second;

		ATLinkProtocol proto;
		proto.addr_width = 16;
		proto.has_valid = true; //ifacedef->has_valid();
		proto.has_ready = true; //ifacedef->has_ready();
		proto.is_packet = true; //ifacedef->has_sop();

		assert(ifacedef->has_sop() == ifacedef->has_eop());

		proto.data_width = 0;
		if (ifacedef->has_data()) proto.data_width += ifacedef->get_data_width();
		if (ifacedef->has_header()) proto.data_width += ifacedef->get_header_width();

		ATNetPort* port;

		switch(ifacedef->get_type())
		{
		case ATInterfaceDef::SEND:
			{
				port = new ATNetOutPort(this);
				port->set_name(ifacedef->get_name());
				add_outport((ATNetOutPort*)port);
			}
			break;

		case ATInterfaceDef::RECV:
			{
				port = new ATNetInPort(this);
				port->set_name(ifacedef->get_name());
				add_inport((ATNetInPort*)port);
			}
			break;

		default:
			assert(false);
		}

		port->set_proto(proto);
		port->set_clock(ifacedef->get_clock());
	}
}


ATInstanceNode::~ATInstanceNode()
{
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

		// haky hack time.
		int addr = 0;
		std::vector<ATNetFlow*> sorted_flows(inport->flows());
		std::sort(sorted_flows.begin(), sorted_flows.end(), [](ATNetFlow* l, ATNetFlow* r)
		{
			return l->get_id() < r->get_id();
		});

		for (ATNetFlow* flow : inport->flows())
		{
			int flow_id = flow->get_id();

			ATLinkDef* link_def = flow->get_def();
			ATLinkPointDef& dest_lp_def = link_def->dest;
			ATEndpointDef* dest_ep_def = spec->get_endpoint_def_for_linkpoint(dest_lp_def);

			int ep_id = dest_ep_def->ep_id;

			ATInstanceNode* src_node = (ATInstanceNode*)flow->get_src_port()->get_parent();
			//int addr = src_node->get_addr();

			pack->define_flow_mapping(flow_id, ep_id, addr++);
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

		// haky hack time.
		int aaddr = 0;
		std::vector<ATNetFlow*> sorted_flows(outport->flows());
		std::sort(sorted_flows.begin(), sorted_flows.end(), [](ATNetFlow* l, ATNetFlow* r)
		{
			return l->get_id() < r->get_id();
		});

		for (ATNetFlow* flow : outport->flows())
		{
			int flow_id = flow->get_id();

			ATLinkDef* link_def = flow->get_def();
			ATLinkPointDef& src_lp_def = link_def->src;
			ATLinkPointDef& dest_lp_def = link_def->dest;
			ATEndpointDef* src_ep_def = spec->get_endpoint_def_for_linkpoint(src_lp_def);
			ATEndpointDef* dest_ep_def = spec->get_endpoint_def_for_linkpoint(dest_lp_def);

			int ep_id = src_ep_def->ep_id;
			int addr = ati_signal_punpack_base::ANY_ADDR;
			
			if (dest_ep_def->type != ATEndpointDef::BROADCAST)
			{
				ATInstanceNode* dest_node = (ATInstanceNode*)flow->get_dest_port()->get_parent();
				//addr = dest_node->get_addr();
				addr = aaddr++;
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