#pragma once

#include <unordered_map>
#include <systemc.h>
#include "ati_channel.h"
#include "at_protocol.h"

class ati_bcast : public sc_module
{
public: 
	ati_send& o_out(int i) { return *m_out[i].send; }
	ati_recv i_in;
	sc_in_clk i_clk;

	ati_bcast(sc_module_name nm, const ATLinkProtocol& proto, int n_outputs);
	~ati_bcast();

	void register_output(int flow_id, int output);

protected:
	struct OutEntry
	{
		ati_send* send;
		sc_signal<bool>* latch;
	};

	typedef std::vector<OutEntry> OutVec;
	typedef std::unordered_map<int, std::vector<OutEntry*>> RouteMap;

	void process_cont();
	void process_clk();

	OutVec m_out;
	RouteMap m_route_map;
	ATLinkProtocol m_proto;
};
