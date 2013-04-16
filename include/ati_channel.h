#pragma once

#include <systemc.h>
#include "sc_bv_signal.h"
#include "at_protocol.h"

class ati_channel
{
public:
	ati_channel(const ATLinkProtocol& proto);
	~ati_channel();

	sc_bv_signal& data() { return *m_data; }
	sc_bv_signal& addr() { return *m_addr; }
	sc_signal<bool>& valid() { return *m_valid; }
	sc_signal<bool>& ready() { return *m_ready; }
	sc_signal<bool>& sop() { return *m_sop; }
	sc_signal<bool>& eop() { return *m_eop; }

	ATLinkProtocol& get_proto() { return m_proto; }

	void trace(const std::string& prefix);

private:
	sc_bv_signal* m_data;
	sc_bv_signal* m_addr;
	sc_signal<bool>* m_valid;
	sc_signal<bool>* m_ready;
	sc_signal<bool>* m_sop;
	sc_signal<bool>* m_eop;

	ATLinkProtocol m_proto;
};


class ati_send
{
public:
	ati_send(const ATLinkProtocol& proto);
	~ati_send();

	sc_out<sc_bv_base>& data() { return *m_data; }
	sc_out<sc_bv_base>& addr() { return *m_addr; }
	sc_out<bool>& valid() { return *m_valid; }
	sc_in<bool>& ready() { return *m_ready; }
	sc_out<bool>& sop() { return *m_sop; }
	sc_out<bool>& eop() { return *m_eop; }

	ATLinkProtocol& get_proto() { return m_proto; }

	void bind(ati_channel&);
	void operator() (ati_channel&);
	void trace(const std::string& prefix);

private:
	sc_out<sc_bv_base>* m_data;
	sc_out<sc_bv_base>* m_addr;
	sc_out<bool>* m_valid;
	sc_in<bool>* m_ready;
	sc_out<bool>* m_sop;
	sc_out<bool>* m_eop;

	ATLinkProtocol m_proto;
};


class ati_recv
{
public:
	ati_recv(const ATLinkProtocol& proto);
	~ati_recv();

	sc_in<sc_bv_base>& data() { return *m_data; }
	sc_in<sc_bv_base>& addr() { return *m_addr; }
	sc_in<bool>& valid() { return *m_valid; }
	sc_out<bool>& ready() { return *m_ready; }
	sc_in<bool>& sop() { return *m_sop; }
	sc_in<bool>& eop() { return *m_eop; }

	ATLinkProtocol& get_proto() { return m_proto; }

	void bind(ati_channel&);
	void operator() (ati_channel&);
	void trace(const std::string& prefix);

private:
	sc_in<sc_bv_base>* m_data;
	sc_in<sc_bv_base>* m_addr;
	sc_in<bool>* m_valid;
	sc_out<bool>* m_ready;
	sc_in<bool>* m_sop;
	sc_in<bool>* m_eop;

	ATLinkProtocol m_proto;
};