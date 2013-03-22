#include "ati_bcast.h"

ati_bcast::ati_bcast(sc_module_name nm, ATLinkProtocol& in_proto, ATLinkProtocol& out_proto, 
					 int n_outputs)
: sc_module(nm), m_n_outputs(n_outputs), i_in(in_proto), m_in_proto(in_proto), 
	m_out_proto(out_proto)
{
	m_out = new ati_send* [n_outputs];
	for (int i = 0; i < n_outputs; i++)
	{
		m_out[i] = new ati_send(out_proto);
	}

	assert(in_proto.addr_width > 0);
	assert(in_proto.has_valid && out_proto.has_valid);

	m_addrs = new sc_bv_base* [n_outputs];
	for (int i = 0; i < n_outputs; i++)
	{
		m_addrs[i] = new sc_bv_base(in_proto.addr_width);
	}

	SC_HAS_PROCESS(ati_bcast);
	SC_METHOD(process);
	
	sensitive << i_in.addr();
	sensitive << i_in.valid();
	if (m_in_proto.data_width> 0) sensitive << i_in.data();
	if (m_in_proto.is_packet)
	{
		sensitive << i_in.sop();
		sensitive << i_in.eop();
	}

	if (m_out_proto.has_ready)
	{
		for (int i = 0; i < m_n_outputs; i++)
		{
			sensitive << m_out[i]->ready();
		}
	}
}


ati_bcast::~ati_bcast()
{
}


void ati_bcast::process()
{
	if (!i_in.valid())
	{
		if (m_in_proto.has_ready) i_in.ready() = true;

		for (int i = 0; i < m_n_outputs; i++)
		{
			m_out[i]->valid() = false;
		}
	}
	else
	{
		for (int i = 0; i < m_n_outputs; i++)
		{
			if (i_in.addr().read() == *m_addrs[i])
			{
				m_out[i]->valid() = true;

				if (m_in_proto.data_width > 0)	m_out[i]->data() = i_in.data();

				if (m_out_proto.has_ready && m_in_proto.has_ready) 
					i_in.ready() = m_out[i]->ready();
				else if (m_in_proto.has_ready) 
					i_in.ready() = true;

				if (m_out_proto.is_packet)
				{
					m_out[i]->sop() = i_in.sop();
					m_out[i]->eop() = i_in.eop();
				}
			}
			else
			{
				m_out[i]->valid() = false;
			}
		}
	}
}


void ati_bcast::set_addr(int i, sc_dt::sc_bv_base *val)
{
	*m_addrs[i] = *val;
}

