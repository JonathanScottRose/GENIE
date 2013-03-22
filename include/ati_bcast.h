#pragma once

#include <systemc.h>
#include "ati_channel.h"
#include "at_protocol.h"

class ati_bcast : public sc_module
{
public: 
	ati_send& o_out(int i) { return *m_out[i]; }
	ati_recv i_in;
	sc_in_clk i_clk;

	ati_bcast(sc_module_name nm, ATLinkProtocol& in_proto, ATLinkProtocol& out_proto, 
		int n_outputs);
	~ati_bcast();

	void set_addr(int i, sc_bv_base* val);

protected:
	void process();

	int m_n_outputs;
	ATLinkProtocol m_in_proto;
	ATLinkProtocol m_out_proto;
	ati_send** m_out;
	sc_bv_base** m_addrs;
};
