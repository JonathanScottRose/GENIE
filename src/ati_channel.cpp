#include "ati_channel.h"

ati_channel::ati_channel(const ATLinkProtocol& proto)
: m_proto(proto)
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

ati_send::ati_send(const ATLinkProtocol& proto)
: m_proto(proto)
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

ati_recv::ati_recv(const ATLinkProtocol& proto)
: m_proto(proto)
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