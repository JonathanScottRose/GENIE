#pragma once

#include <systemc.h>
#include "at_protocol.h"
#include "ati_channel.h"


class ati_proto_adapter : public sc_module
{
public:
	ati_recv i_in;
	ati_send o_out;

	ati_proto_adapter(sc_module_name nm, const ATLinkProtocol& src_proto,
		const ATLinkProtocol& dest_proto);
	~ati_proto_adapter();

	void set_addr(const sc_bv_base& addr);

private:
	void process();

	sc_bv_base* m_const_addr;
	ATLinkProtocol m_src_proto;
	ATLinkProtocol m_dest_proto;
};
