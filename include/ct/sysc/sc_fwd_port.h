#pragma once

#include <sysc/communication/sc_signal_ports.h>
#include "sc_bv_signal.h"

namespace ct
{
namespace SysC
{

template<int W>
class sc_fwd_in : 
	virtual public sc_signal_in_if<sc_bv<W>>
{
public:
	sc_fwd_in(sc_signal_in_if<sc_bv_base>* src)
		: m_src(src) { }

	virtual ~sc_fwd_in() { }

	virtual const sc_event& value_changed_event() const { return m_src->value_changed_event(); }
	virtual const sc_bv<W>& read() const { return (sc_bv<W>&)(m_src->read()); }
	virtual const sc_bv<W>& get_data_ref() const { return (sc_bv<W>&)(m_src->get_data_ref()); }
	virtual bool event() const { return m_src->event(); }

protected:
	sc_signal_in_if<sc_bv_base>* m_src;
};


template<int W>
class sc_fwd_out : 
	public sc_fwd_in<W>,
	virtual public sc_signal_write_if<sc_bv<W>>
{
public:
	sc_fwd_out(sc_signal_inout_if<sc_bv_base>* sink)
		: m_sink(sink) { }

	virtual ~sc_fwd_out() { }

	virtual void write (const sc_bv<W>& val) { m_sink->write( (sc_bv_base&)val ); }
		

protected:
	sc_signal_inout_if<sc_bv_base>* m_sink;
};

}
}