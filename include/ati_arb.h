#pragma once

#include <systemc.h>
#include "ati_channel.h"
#include "at_protocol.h"

class ati_arb : public sc_module
{
public:
	ati_recv& i_in(int i) { return *m_in[i]; }
	ati_send o_out;
	sc_in_clk i_clk;

	ati_arb(sc_module_name nm, ATLinkProtocol& in_proto, ATLinkProtocol& out_proto, int n_inputs);
	~ati_arb();

protected:
	void process_cont();
	void process_clk();

	bool find_next_input();

	int m_n_inputs;
	sc_signal<int> m_cur_input;
	ATLinkProtocol m_in_proto;
	ATLinkProtocol m_out_proto;
	ati_recv** m_in;
};