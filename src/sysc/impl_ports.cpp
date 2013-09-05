#include "ati_channel.h"
#include "at_manager.h"

ati_channel::ati_channel(const ATLinkProtocol& proto)
: m_proto(proto), m_data(nullptr), m_addr(nullptr), m_valid(nullptr), m_ready(nullptr),
	m_sop(nullptr), m_eop(nullptr)
{
	if (proto.data_width > 0)
		m_data = new sc_bv_signal(proto.data_width);
	if (proto.addr_width > 0)
		m_addr = new sc_bv_signal(proto.addr_width);
	if (proto.has_valid)
		m_valid = new sc_signal<bool>();
	if (proto.has_ready)
		m_ready = new sc_signal<bool>();

	if (proto.is_packet)
	{
		m_sop = new sc_signal<bool>();
		m_eop = new sc_signal<bool>();
	}
}

ati_channel::~ati_channel()
{
	if (m_proto.data_width > 0) delete m_data;
	if (m_proto.addr_width > 0) delete m_addr;
	if (m_proto.has_valid) delete m_valid;
	if (m_proto.has_ready) delete m_ready;
	if (m_proto.is_packet)
	{
		delete m_sop;
		delete m_eop;
	}
}

void ati_channel::trace(const std::string& prefix)
{
	sc_trace_file* tf = ATManager::inst()->get_trace_file();
	if (m_proto.has_valid) sc_trace(tf, valid(), prefix + "_valid");
	if (m_proto.has_ready) sc_trace(tf, ready(), prefix + "_ready");
	if (m_proto.is_packet) sc_trace(tf, sop(), prefix + "_sop");
	if (m_proto.is_packet) sc_trace(tf, eop(), prefix + "_eop");
	if (m_proto.data_width > 0) sc_trace(tf, data(), prefix + "_data");
	if (m_proto.addr_width > 0) sc_trace(tf, addr(), prefix + "_flow");
}

ati_send::ati_send(const ATLinkProtocol& proto)
: m_proto(proto), m_data(nullptr), m_addr(nullptr), m_valid(nullptr), m_ready(nullptr),
	m_sop(nullptr), m_eop(nullptr)
{
	if (m_proto.data_width > 0) m_data = new sc_out<sc_bv_base>();
	if (m_proto.addr_width > 0) m_addr = new sc_out<sc_bv_base>();
	if (m_proto.has_ready) m_ready = new sc_in<bool>();
	if (m_proto.has_valid) m_valid = new sc_out<bool>();
	if (m_proto.is_packet)
	{
		m_sop = new sc_out<bool>();
		m_eop = new sc_out<bool>();
	}
}

ati_send::~ati_send()
{
	if (m_proto.data_width > 0) delete m_data;
	if (m_proto.addr_width > 0) delete m_addr;
	if (m_proto.has_ready) delete m_ready;
	if (m_proto.has_valid) delete m_valid;
	if (m_proto.is_packet)
	{
		delete m_sop;
		delete m_eop;
	}
}

void ati_send::bind(ati_channel& c)
{
	if (m_proto.data_width > 0) m_data->bind(c.data());
	if (m_proto.addr_width > 0) m_addr->bind(c.addr());
	if (m_proto.has_valid) m_valid->bind(c.valid());
	if (m_proto.has_ready) m_ready->bind(c.ready());
	if (m_proto.is_packet)
	{
		m_sop->bind(c.sop());
		m_eop->bind(c.eop());
	}
}

void ati_send::operator ()(ati_channel &c)
{
	bind(c);
}

void ati_send::trace(const std::string& prefix)
{
	sc_trace_file* tf = ATManager::inst()->get_trace_file();
	if (m_proto.has_valid) sc_trace(tf, valid(), prefix + "_valid");
	if (m_proto.has_ready) sc_trace(tf, ready(), prefix + "_ready");
	if (m_proto.is_packet) sc_trace(tf, sop(), prefix + "_sop");
	if (m_proto.is_packet) sc_trace(tf, eop(), prefix + "_eop");
	if (m_proto.data_width > 0) sc_trace(tf, data(), prefix + "_data");
	if (m_proto.addr_width > 0) sc_trace(tf, addr(), prefix + "_flow");
}

ati_recv::ati_recv(const ATLinkProtocol& proto)
: m_proto(proto), m_data(nullptr), m_addr(nullptr), m_valid(nullptr), m_ready(nullptr), 
	m_sop(nullptr), m_eop(nullptr)
{
	if (m_proto.data_width > 0) m_data = new sc_in<sc_bv_base>();
	if (m_proto.addr_width > 0) m_addr = new sc_in<sc_bv_base>();
	if (m_proto.has_ready) m_ready = new sc_out<bool>();
	if (m_proto.has_valid) m_valid = new sc_in<bool>();
	if (m_proto.is_packet)
	{
		m_sop = new sc_in<bool>();
		m_eop = new sc_in<bool>();
	}
}

ati_recv::~ati_recv()
{
	if (m_proto.data_width > 0) delete m_data;
	if (m_proto.addr_width > 0) delete m_addr;
	if (m_proto.has_ready) delete m_ready;
	if (m_proto.has_valid) delete m_valid;
	if (m_proto.is_packet)
	{
		delete m_sop;
		delete m_eop;
	}
}

void ati_recv::bind(ati_channel& c)
{
	if (m_proto.data_width > 0) m_data->bind(c.data());
	if (m_proto.addr_width > 0) m_addr->bind(c.addr());
	if (m_proto.has_valid) m_valid->bind(c.valid());
	if (m_proto.has_ready) m_ready->bind(c.ready());
	if (m_proto.is_packet)
	{
		m_sop->bind(c.sop());
		m_eop->bind(c.eop());
	}
}

void ati_recv::operator ()(ati_channel &c)
{
	bind(c);
}

void ati_recv::trace(const std::string& prefix)
{
	sc_trace_file* tf = ATManager::inst()->get_trace_file();
	if (m_proto.has_valid) sc_trace(tf, valid(), prefix + "_valid");
	if (m_proto.has_ready) sc_trace(tf, ready(), prefix + "_ready");
	if (m_proto.is_packet) sc_trace(tf, sop(), prefix + "_sop");
	if (m_proto.is_packet) sc_trace(tf, eop(), prefix + "_eop");
	if (m_proto.data_width > 0) sc_trace(tf, data(), prefix + "_data");
	if (m_proto.addr_width > 0) sc_trace(tf, addr(), prefix + "_flow");
}