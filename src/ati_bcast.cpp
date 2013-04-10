#include "ati_bcast.h"

ati_bcast::ati_bcast(sc_module_name nm, const ATLinkProtocol& proto, int n_outputs)
: sc_module(nm), m_proto(proto), i_in(proto)
{
	SC_HAS_PROCESS(ati_bcast);
	SC_METHOD(process);
	
	m_out.resize(n_outputs);
	for (auto& send : m_out)
	{
		send = new ati_send(proto);
	}

	sensitive << i_in.addr();
	sensitive << i_in.valid();
	if (m_proto.data_width > 0) sensitive << i_in.data();
	if (m_proto.is_packet)
	{
		sensitive << i_in.sop();
		sensitive << i_in.eop();
	}

	if (m_proto.has_ready)
	{
		for (auto output : m_out)
		{
			sensitive << output->ready();
		}
	}
}


ati_bcast::~ati_bcast()
{
}


void ati_bcast::register_output(int flow_id, int output)
{
	m_route_map[flow_id].push_back(m_out[output]);
}


void ati_bcast::process()
{
	for (ati_send* output : m_out)
	{
		output->valid() = false;
	}

	i_in.ready() = true;

	if (i_in.valid())
	{
		int flow_id = i_in.addr().read().to_int();

		bool all_ready = true;
		for (ati_send* output : m_route_map[flow_id])
		{
			all_ready &= output->ready();
		}

		i_in.ready() = all_ready;

		for (ati_send* output : m_route_map[flow_id])
		{
			output->data() = i_in.data();
			output->valid() = all_ready;
			output->sop() = i_in.sop();
			output->eop() = i_in.eop();
			output->addr() = i_in.addr();
		}
	}
}


