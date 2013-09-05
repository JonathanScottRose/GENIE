#include "ati_arb.h"

ati_arb::ati_arb(sc_module_name nm, ATLinkProtocol& in_proto, ATLinkProtocol& out_proto, 
				 int n_inputs)
: sc_module(nm), o_out(out_proto),
	m_out_proto(out_proto), m_in_proto(in_proto)
{
	m_in.resize(n_inputs);

	for (auto& input : m_in)
	{
		input.in = new ati_recv(in_proto);
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
	sensitive << m_state;
	sensitive << o_out.ready();

	for (auto& input : m_in)
	{
		if (m_in_proto.data_width > 0) sensitive << input.in->data();
		sensitive << input.in->valid();
		sensitive << input.in->sop();
		sensitive << input.in->eop();
	}
}


ati_arb::~ati_arb()
{
}


bool ati_arb::find_next_input()
{
	bool found = false;
	int next = m_cur_input;
	for (int i = 0; i < (int)m_in.size(); i++)
	{
		next = (next + 1) % m_in.size();
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
	ati_recv* cur_in = m_in[m_cur_input].in;
	int flow_id = cur_in->addr().read().to_int();

	o_out.addr() = cur_in->addr();
	o_out.valid() = (m_state == State::TRANSMIT) && cur_in->valid();
	if (m_out_proto.data_width > 0) o_out.data() = cur_in->data();
	o_out.sop() = cur_in->sop();
	o_out.eop() = cur_in->eop();

	for (int i = 0; i < (int)m_in.size(); i++)
	{
		ati_recv* in = m_in[i].in;
		in->ready() = (m_state == State::TRANSMIT) && (m_cur_input == i) && o_out.ready();
	}
}


void ati_arb::process_clk()
{
	m_state = State::IDLE;

	while(true)
	{
		wait();

		ati_recv* cur_in = m_in[m_cur_input].in;

		switch (m_state)
		{
		case State::IDLE:
			if (find_next_input()) m_state = State::TRANSMIT;
			break;

		case State::TRANSMIT:
			if (cur_in->valid() && o_out.ready() && cur_in->eop())
			{
				m_state = State::IDLE;
			}
			break;
		}
	}
}

#include "at_manager.h"
void ati_arb::trace_internal(const std::string& prefix)
{
	sc_trace(ATManager::inst()->get_trace_file(), m_cur_input, prefix + "cur_input");
	sc_trace(ATManager::inst()->get_trace_file(), m_state, prefix + "state");
}

