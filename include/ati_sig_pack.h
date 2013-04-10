#pragma once

#include <systemc.h>
#include "at_netlist.h"
#include "sc_bv_signal.h"

class ati_signal_punpack_base : public sc_module
{
public:
	static const int ANY_ADDR = -1;

	ati_signal_punpack_base(const ATLinkProtocol& proto, sc_module_name name);
	~ati_signal_punpack_base();

	void add_valid_signal(sc_signal<bool>* sig) { m_valid_sig = sig; }
	void add_ready_signal(sc_signal<bool>* sig) { m_ready_sig = sig; }
	void add_sop_signal(sc_signal<bool>* sig) { m_sop_sig = sig; }
	void add_eop_signal(sc_signal<bool>* sig) { m_eop_sig = sig; }
	void add_data_signal(sc_bv_signal* sig, int index, int size);
	void add_addr_signal(sc_bv_signal* sig) { m_addr_sig = sig; }
	void add_epid_signal(sc_bv_signal* sig) { m_epid_sig = sig; }

	void define_flow_mapping(int flow_id, int ep_id, int addr = ANY_ADDR);

protected:
	struct SigEntry
	{
		sc_bv_signal* sig;
		int idx;
		int width;
	};

	struct FlowMapping
	{
		int ep_id;
		int addr;
	};

	typedef std::vector<SigEntry*> SigEntryVec;
	typedef std::unordered_map<int, FlowMapping> FlowMap;

	int find_by_mapping(int epid, int addr);
	const FlowMapping* find_by_id(int flow_id);

	sc_signal<bool>* m_valid_sig;
	sc_signal<bool>* m_ready_sig;
	sc_signal<bool>* m_sop_sig;
	sc_signal<bool>* m_eop_sig;
	sc_bv_signal* m_addr_sig;
	sc_bv_signal* m_epid_sig;
	SigEntryVec m_data_sigs;

	ATLinkProtocol m_proto;
	FlowMap m_flow_map;
};


class ati_signal_packer : public ati_signal_punpack_base
{
public:
	ati_send& send() { return *m_send; }

	ati_signal_packer(const ATLinkProtocol& proto, sc_module_name name);
	~ati_signal_packer();

private:
	void process();

	ati_send* m_send;
	sc_bv_base* m_value;
	sc_event_or_list m_change_event;
};


class ati_signal_unpacker : public ati_signal_punpack_base
{
public:
	ati_recv& recv() { return *m_recv; }

	ati_signal_unpacker(const ATLinkProtocol& proto, sc_module_name name);
	~ati_signal_unpacker();

private:
	void process();

	ati_recv* m_recv;
	sc_event_or_list m_change_event;
};


