#include "ati_sig_pack.h"
#include "at_util.h"
#include "ati_channel.h"

//
// Base
//

ati_signal_punpack_base::ati_signal_punpack_base
	(const ATLinkProtocol& proto, sc_module_name name)
	: sc_module(name), m_proto(proto)
{
	m_valid_sig = nullptr;
	m_ready_sig = nullptr;
	m_sop_sig = nullptr;
	m_eop_sig = nullptr;
	m_addr_sig = nullptr;
	m_epid_sig = nullptr;
}

ati_signal_punpack_base::~ati_signal_punpack_base()
{
	at_delete_all(m_data_sigs);
}

void ati_signal_punpack_base::add_data_signal(sc_bv_signal* sig, int index, int size)
{
	SigEntry* entry = new SigEntry;
	entry->sig = sig;
	entry->idx = index;
	entry->width = size;
	m_data_sigs.push_back(entry);
}

void ati_signal_punpack_base::define_flow_mapping(int flow_id, int ep_id, int addr)
{
	FlowMapping mp = {ep_id, addr};
	m_flow_map[flow_id] = mp;
}

int ati_signal_punpack_base::find_by_mapping(int epid, int addr)
{
	for (auto& i : m_flow_map)
	{
		if (i.second.ep_id == epid)
		{
			if (i.second.addr == ANY_ADDR ||
				i.second.addr == addr)
			{
				return i.first;
				
			}
		}
	}

	assert(false);
	return -1;
}

auto ati_signal_punpack_base::find_by_id(int flow_id) -> const FlowMapping*
{
	return & (m_flow_map[flow_id]);
}

//
// Pack
//

ati_signal_packer::ati_signal_packer(const ATLinkProtocol& proto, sc_module_name name)
: ati_signal_punpack_base(proto, name)
{
	SC_HAS_PROCESS(ati_signal_packer);
	SC_THREAD(process);

	m_send = new ati_send(proto);

	int size = proto.data_width;
	if (size > 0)
	{
		m_value = new sc_bv_base(size);
	}
}


ati_signal_packer::~ati_signal_packer()
{
	delete m_send;
	if (m_value)
		delete m_value;
}


void ati_signal_packer::process()
{
	sc_bv_base flow_bv(16);

	// Set up sensitivities
	if (m_addr_sig)	m_change_event |= m_addr_sig->value_changed_event();
	if (m_epid_sig) m_change_event |= m_epid_sig->value_changed_event();
	if (m_ready_sig) m_change_event |= m_change_event |= m_send->ready().value_changed_event();
	if (m_valid_sig) m_change_event |= m_valid_sig->value_changed_event();
	if (m_sop_sig) m_change_event |= m_sop_sig->value_changed_event();
	if (m_eop_sig) m_change_event |= m_eop_sig->value_changed_event();

	for (SigEntry* entry : m_data_sigs)
	{
		m_change_event |= entry->sig->value_changed_event();
	}

	// Set default values for unspecified signals
	if (!m_addr_sig && !m_epid_sig)
	{
		assert(m_flow_map.size() == 1);

		auto& mapping = m_flow_map.begin();
		flow_bv = mapping->first;
		m_send->addr() = flow_bv;
	}

	if (!m_valid_sig) m_send->valid() = true;
	if (!m_sop_sig) m_send->sop() = true;
	if (!m_eop_sig) m_send->eop() = true;

	while(true)
	{
		wait(m_change_event);

		if (m_valid_sig && m_valid_sig->event())
		{
			m_send->valid() = *m_valid_sig;
		}

		if (m_ready_sig && m_send->ready().event())
		{
			*m_ready_sig = m_send->ready();
		}

		if (m_addr_sig || m_epid_sig)
		{
			int addr = m_addr_sig? m_addr_sig->read().to_int() : 0;
			int epid = m_epid_sig? m_epid_sig->read().to_int() : 0;

			flow_bv = find_by_mapping(epid, addr);
			m_send->addr() = flow_bv;
		}

		if (m_sop_sig && (m_sop_sig->event()))
		{
			m_send->sop() = *m_sop_sig;
		}

		if (m_eop_sig && (m_eop_sig->event()))
		{
			m_send->eop() = *m_eop_sig;
		}

		bool change_data = false;
		for (SigEntry* entry : m_data_sigs)
		{
			if (!entry->sig->event())
				continue;

			change_data = true;
			const sc_bv_base& val = entry->sig->read();
			m_value->range(entry->idx + entry->width-1, entry->idx) = val;
		}

		if (change_data)
		{
			m_send->data() = *m_value;
		}
	}
}


//
// Unpack
//

ati_signal_unpacker::ati_signal_unpacker(const ATLinkProtocol& proto, sc_module_name name)
: ati_signal_punpack_base(proto, name)
{
	m_recv = new ati_recv(proto);

	SC_HAS_PROCESS(ati_signal_unpacker);
	SC_THREAD(process);
}


ati_signal_unpacker::~ati_signal_unpacker()
{
	delete m_recv;
	at_delete_all(m_data_sigs);
}


void ati_signal_unpacker::process()
{
	if (!m_data_sigs.empty()) m_change_event |= m_recv->data().value_changed_event();
	if (m_epid_sig || m_addr_sig) m_change_event |= m_recv->addr().value_changed_event();
	if (m_ready_sig) m_change_event |=  m_ready_sig->value_changed_event();
	if (m_valid_sig) m_change_event |=  m_recv->valid().value_changed_event();
	if (m_sop_sig) m_change_event |=  m_recv->sop().value_changed_event();
	if (m_eop_sig) m_change_event |=  m_recv->eop().value_changed_event();

	if (!m_ready_sig) m_recv->ready() = true;

	while(true)
	{
		wait(m_change_event);

		if ((m_addr_sig || m_epid_sig) && m_recv->addr().event())
		{
			// Decode

			int flow_id = m_recv->addr().read().to_int();
			const FlowMapping* mapping = find_by_id(flow_id);
			
			sc_bv_base asdf;
			asdf = mapping->addr;
			sc_bv_base fdsa;
			fdsa = mapping->ep_id;

			if (m_addr_sig) m_addr_sig->write(asdf);
			if (m_epid_sig) m_epid_sig->write(fdsa);
		}

		if (m_ready_sig && m_ready_sig->event())
		{
			m_recv->ready() = *m_ready_sig;
		}

		if (m_valid_sig && m_recv->valid().event())
		{
			*m_valid_sig = m_recv->valid();
		}

		if (m_sop_sig && m_recv->sop().event())
		{
			*m_sop_sig = m_recv->sop();
		}

		if (m_eop_sig && m_recv->eop().event())
		{
			*m_eop_sig = m_recv->eop();
		}

		if (m_proto.data_width > 0 && m_recv->data().event())
		{
			for (SigEntry* entry : m_data_sigs)
			{
				sc_bv_signal& sig = *(entry->sig);
				sig.write(m_recv->data().read().range(entry->idx + entry->width-1, entry->idx));
			}
		}
	}
}

