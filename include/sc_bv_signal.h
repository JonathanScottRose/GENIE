#pragma once

#include <sysc/communication/sc_signal.h>
#include <sysc/datatypes/bit/sc_bv_base.h>

class sc_bv_signal : public sc_signal<sc_bv_base>
{
public:
	sc_bv_signal(int width)
		: sc_signal()
	{
		m_cur_val.~sc_bv_base();
		m_new_val.~sc_bv_base();
		new (&m_cur_val) sc_bv_base(width);
		new (&m_new_val) sc_bv_base(width);
	}
};


