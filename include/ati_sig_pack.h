#pragma once

#include <systemc.h>
#include "at_netlist.h"
#include "sc_bv_signal.h"

class ati_signal_packer : public sc_module
{
public:
	ati_send& send() { return *m_send; }

	ati_signal_packer(const ATLinkProtocol& proto, sc_module_name name);
	~ati_signal_packer();

	void add_valid_signal(sc_signal<bool>* sig) { m_valid_sig = sig; }
	void add_ready_signal(sc_signal<bool>* sig) { m_ready_sig = sig; }
	void add_sop_signal(sc_signal<bool>* sig) { m_sop_sig = sig; }
	void add_eop_signal(sc_signal<bool>* sig) { m_eop_sig = sig; }
	void add_data_signal(sc_bv_signal* sig, int index, int size);
	void add_addr_signal(sc_bv_signal* sig) { m_addr_sig = sig; }

private:
	struct SigEntry
	{
		sc_bv_signal* sig;
		int idx;
		int width;
	};

	typedef std::vector<SigEntry*> SigEntryVec;

	void process();

	ati_send* m_send;
	sc_signal<bool>* m_valid_sig;
	sc_signal<bool>* m_ready_sig;
	sc_signal<bool>* m_sop_sig;
	sc_signal<bool>* m_eop_sig;
	sc_bv_signal* m_addr_sig;
	SigEntryVec m_data_sigs;

	sc_bv_base* m_value;
	sc_event_or_list m_change_event;
	ATLinkProtocol m_proto;
};


class ati_signal_unpacker : public sc_module
{
public:
	ati_recv& recv() { return *m_recv; }

	ati_signal_unpacker(const ATLinkProtocol& proto, sc_module_name name);
	~ati_signal_unpacker();

	void add_valid_signal(sc_signal<bool>* sig) { m_valid_sig = sig; }
	void add_ready_signal(sc_signal<bool>* sig) { m_ready_sig = sig; }
	void add_sop_signal(sc_signal<bool>* sig) { m_sop_sig = sig; }
	void add_eop_signal(sc_signal<bool>* sig) { m_eop_sig = sig; }
	void add_data_signal(sc_bv_signal* sig, int index, int size);
	void add_addr_signal(sc_bv_signal* sig) { m_addr_sig = sig; }

private:
	struct SigEntry
	{
		sc_bv_signal* sig;
		int idx;
		int width;
	};

	typedef std::vector<SigEntry*> SigEntryVec;

	void process();

	ati_recv* m_recv;
	sc_signal<bool>* m_valid_sig;
	sc_signal<bool>* m_ready_sig;
	sc_signal<bool>* m_sop_sig;
	sc_signal<bool>* m_eop_sig;
	sc_bv_signal* m_addr_sig;
	SigEntryVec m_data_sigs;

	ATLinkProtocol m_proto;
	sc_event_or_list m_change_event;
};


