#include "ati_sig_pack.h"
#include "at_util.h"
#include "ati_channel.h"

ati_signal_packer::ati_signal_packer(const ATLinkProtocol& proto, sc_module_name name)
: sc_module(name), m_proto(proto), m_value(0)
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

	at_delete_all(m_data_sigs);
}


void ati_signal_packer::add_data_signal(sc_bv_signal* sig, int index, int size)
{
	SigEntry* entry = new SigEntry;
	entry->sig = sig;
	entry->idx = index;
	entry->width = size;
	m_data_sigs.push_back(entry);
}


void ati_signal_packer::process()
{
	if (m_proto.addr_width > 0)
	{
		m_change_event |= m_addr_sig->value_changed_event();
	}

	if (m_proto.has_ready)
	{
		m_change_event |= m_send->ready().value_changed_event();
	}

	if (m_proto.has_valid)
	{
		m_change_event |= m_valid_sig->value_changed_event();
	}

	if (m_proto.is_packet)
	{
		m_change_event |= m_sop_sig->value_changed_event();
		m_change_event |= m_eop_sig->value_changed_event();
	}

	for (SigEntryVec::iterator i = m_data_sigs.begin(); i != m_data_sigs.end(); ++i)
	{
		SigEntry* entry = *i;
		m_change_event |= entry->sig->value_changed_event();
	}

	while(true)
	{
		wait(m_change_event);

		if (m_proto.has_valid && m_valid_sig->event())
		{
			m_send->valid() = *m_valid_sig;
		}

		if (m_proto.has_ready && m_send->ready().event())
		{
			*m_ready_sig = m_send->ready();
		}

		if (m_proto.addr_width > 0 && m_addr_sig->event())
		{
			m_send->addr() = *m_addr_sig;
		}

		if (m_proto.is_packet && (m_sop_sig->event()))
		{
			m_send->sop() = *m_sop_sig;
		}

		if (m_proto.is_packet && (m_eop_sig->event()))
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
// 
//




ati_signal_unpacker::ati_signal_unpacker(const ATLinkProtocol& proto, sc_module_name name)
: sc_module(name), m_proto(proto)
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


void ati_signal_unpacker::add_data_signal(sc_bv_signal* sig, int index, int size)
{
	SigEntry* entry = new SigEntry;
	entry->sig = sig;
	entry->idx = index;
	entry->width = size;
	m_data_sigs.push_back(entry);
}


void ati_signal_unpacker::process()
{
	if (m_proto.data_width > 0)
	{
		m_change_event |= m_recv->data().value_changed_event();
	}

	if (m_proto.addr_width > 0)
	{
		m_change_event |= m_recv->addr().value_changed_event();
	}

	if (m_proto.has_ready)
	{
		m_change_event |=  m_ready_sig->value_changed_event();
	}

	if (m_proto.has_valid)
	{
		m_change_event |=  m_recv->valid().value_changed_event();
	}

	if (m_proto.is_packet)
	{
		m_change_event |=  m_recv->sop().value_changed_event();
		m_change_event |=  m_recv->eop().value_changed_event();
	}

	while(true)
	{
		wait(m_change_event);

		if (m_proto.addr_width > 0 && m_recv->addr().event())
		{
			m_addr_sig->write(m_recv->addr());
		}

		if (m_proto.has_ready && m_ready_sig->event())
		{
			m_recv->ready() = *m_ready_sig;
		}

		if (m_proto.has_valid && m_recv->valid().event())
		{
			*m_valid_sig = m_recv->valid();
		}

		if (m_proto.is_packet && m_recv->sop().event())
		{
			*m_sop_sig = m_recv->sop();
		}

		if (m_proto.is_packet && m_recv->eop().event())
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

