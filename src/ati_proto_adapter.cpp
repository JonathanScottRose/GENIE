#include "ati_proto_adapter.h"


ati_proto_adapter::ati_proto_adapter(sc_module_name nm, const ATLinkProtocol &src_proto, 
									 const ATLinkProtocol &dest_proto)
: sc_module(nm), m_src_proto(src_proto), m_dest_proto(dest_proto), i_in(src_proto), o_out(dest_proto)
{
	if (m_src_proto.data_width != m_dest_proto.data_width) throw std::runtime_error("Data widths must match");
	if (m_src_proto.has_valid && !m_dest_proto.has_valid) throw std::runtime_error("Incompatible valids");
	if (!m_src_proto.has_ready && m_dest_proto.has_ready) throw std::runtime_error("Incompatible backpressure");

	if (m_src_proto.addr_width == 0 && m_dest_proto.addr_width > 0)
	{
		m_const_addr = new sc_bv_base(m_dest_proto.addr_width);
	}

	SC_HAS_PROCESS(ati_proto_adapter);
	SC_METHOD(process);
	
	if (src_proto.data_width > 0) sensitive << i_in.data();
	if (src_proto.addr_width > 0) sensitive << i_in.addr();
	if (src_proto.has_valid) sensitive << i_in.valid();
	if (dest_proto.has_ready) sensitive << o_out.ready();
	if (src_proto.is_packet) sensitive << i_in.sop() << i_in.eop();
}


ati_proto_adapter::~ati_proto_adapter()
{
	if (m_src_proto.addr_width == 0 && m_dest_proto.addr_width > 0) delete m_const_addr;
}


void ati_proto_adapter::set_addr(const sc_bv_base &addr)
{
	if (m_src_proto.addr_width == 0 && m_dest_proto.addr_width > 0) *m_const_addr = addr;
}


void ati_proto_adapter::process()
{
	if (m_src_proto.addr_width == 0 && m_dest_proto.addr_width > 0) o_out.addr() = *m_const_addr;
	else if (m_src_proto.addr_width > 0 && m_dest_proto.addr_width > 0) 
	{
		o_out.addr() = i_in.addr();
	}

	if (m_src_proto.data_width > 0) o_out.data() = i_in.data();

	if (!m_src_proto.has_valid && m_dest_proto.has_valid) o_out.valid() = true;
	else if (m_dest_proto.has_valid) o_out.valid() = i_in.valid();

	if (m_src_proto.has_ready && !m_dest_proto.has_ready) i_in.ready() = true;
	else if (m_src_proto.has_ready) i_in.ready() = o_out.ready();

	if (!m_src_proto.is_packet && m_dest_proto.is_packet) o_out.sop() = true, o_out.eop() = true;
	else if (m_dest_proto.is_packet) o_out.sop() = i_in.sop(), o_out.eop() = i_in.eop();
}