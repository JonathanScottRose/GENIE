#include "ati_arb.h"

ati_arb::ati_arb(sc_module_name nm, ATLinkProtocol& in_proto, ATLinkProtocol& out_proto, 
				 int n_inputs)
: sc_module(nm), m_n_inputs(n_inputs), o_out(out_proto),
	m_out_proto(out_proto), m_in_proto(in_proto)
{
	m_in = new ati_recv* [n_inputs];
	for (int i = 0; i < n_inputs; i++)
	{
		m_in[i] = new ati_recv(in_proto);
	}

	assert(out_proto.addr_width > 0);
	assert(in_proto.has_valid && out_proto.has_valid);
	assert(in_proto.has_ready && out_proto.has_ready);

	SC_HAS_PROCESS(ati_arb);
	SC_THREAD(process_clk);
	sensitive << i_clk.pos();
	dont_initialize();

	SC_METHOD(process_cont);
	sensitive << m_cur_input;
	sensitive << o_out.ready();
	for (int i = 0; i < n_inputs; i++)
	{
		if (m_in_proto.data_width > 0) sensitive << m_in[i]->data();
		sensitive << m_in[i]->valid();
		sensitive << m_in[i]->sop();
		sensitive << m_in[i]->eop();
	}
}


ati_arb::~ati_arb()
{
	for (int i = 0; i < m_n_inputs; i++)
	{
		delete m_in[i];
	}

	delete[] m_in;
}


bool ati_arb::find_next_input()
{
	bool found = false;
	int next = m_cur_input;
	for (int i = 0; i < m_n_inputs; i++)
	{
		next = (next + 1) % m_n_inputs;
		if (i_in(next).valid())
		{
			found = true;
			m_cur_input = next;
			break;
		}
	}

	return found;
}


void ati_arb::process_cont()
{
	ati_recv* cur_in = m_in[m_cur_input];

	int flow_id = cur_in->addr().read().to_int();

	o_out.addr() = cur_in->addr();
	o_out.valid() = cur_in->valid();
	if (m_out_proto.data_width > 0) o_out.data() = cur_in->data();
	if (m_out_proto.is_packet)
	{
		o_out.sop() = cur_in->sop();
		o_out.eop() = cur_in->eop();
	}

	for (int i = 0; i < m_n_inputs; i++)
	{
		if (i == m_cur_input) cur_in->ready() = o_out.ready();
		else m_in[i]->ready() = false;
	}
}


void ati_arb::process_clk()
{
	sc_event_or_list any_valid;

	for (int i = 0; i < m_n_inputs; i++)
	{
		any_valid |= m_in[i]->valid().value_changed_event();
	}

	while(true)
	{
		wait();

		while (!find_next_input())
		{
			wait(any_valid);
			wait();
		}

		ati_recv* cur_in = m_in[m_cur_input.get_new_value()];

		while ( !(cur_in->valid() && cur_in->ready() && cur_in->eop()) )
		{
			wait();
		}
	}
}

