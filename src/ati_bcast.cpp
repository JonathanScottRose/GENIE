#include "ati_bcast.h"

ati_bcast::ati_bcast(sc_module_name nm, const ATLinkProtocol& proto, int n_outputs)
: sc_module(nm), m_proto(proto), i_in(proto)
{
	SC_HAS_PROCESS(ati_bcast);
	SC_THREAD(process_clk);
	sensitive << i_clk;

	SC_METHOD(process_cont);
	
	m_out.resize(n_outputs);
	for (auto& out : m_out)
	{
		out.send = new ati_send(proto);
		out.latch = new sc_signal<bool>();
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
			sensitive << output.send->ready();
		}
	}
}


ati_bcast::~ati_bcast()
{
}


void ati_bcast::register_output(int flow_id, int output)
{
	m_route_map[flow_id].push_back(&m_out[output]);
}


void ati_bcast::process_clk()
{
	while(true)
	{
		int flow_id = i_in.addr().read().to_int();

		bool all_ready = true;
		for (auto out : m_route_map[flow_id])
		{
			ati_send* output = out->send;
			all_ready &= (output->ready() | *out->latch);
		}

		if (i_in.valid())
		{
			for (auto out : m_route_map[flow_id])
			{
				ati_send* output = out->send;
				if (output->ready() && !all_ready) *out->latch = true;
				else if (all_ready) *out->latch = false;
			}
		}

		wait();		
	}
}


void ati_bcast::process_cont()
{
	for (auto out : m_out)
	{
		out.send->valid() = false;
		out.latch = false;
	}

	i_in.ready() = true;

	if (i_in.valid())
	{
		int flow_id = i_in.addr().read().to_int();

		bool all_ready = true;
		for (auto out : m_route_map[flow_id])
		{
			ati_send* output = out->send;
			all_ready &= (output->ready() | *out->latch);
		}

		i_in.ready() = all_ready;

		for (auto out : m_route_map[flow_id])
		{
			ati_send* output = out->send;
			output->data() = i_in.data();
			output->valid() = i_in.valid() & !*out->latch;
			output->sop() = i_in.sop();
			output->eop() = i_in.eop();
			output->addr() = i_in.addr();
		}
	}
}


