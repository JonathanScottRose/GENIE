#pragma once

#include <sysc/communication/sc_signal_ifs.h>
#include <sysc/datatypes/bit/sc_bv_base.h>

template <int W>
class sc_bv_out : public sc_out<sc_bv_base>
{
public:
	typedef sc_out<sc_bv_base>                            base_type;
	typedef typename base_type::inout_if_type			  inout_if_type;
	typedef typename base_type::inout_port_type           inout_port_type;
	typedef sc_bv_out<W>                                  this_type;
	
	sc_bv_out() 
		: base_type() {}

	explicit sc_bv_out( const char* name ) 
		: base_type(name) {}

	sc_bv_out( const char* name_, inout_if_type& interface_ )
		: base_type( name_, interface_ ) {}

	explicit sc_bv_out( inout_port_type& parent_ )
		: base_type( parent_ )	{}

	sc_bv_out( const char* name_, inout_port_type& parent_ )
		: base_type( name_, parent_ )	{}

    sc_bv_out( this_type& parent_ )
		: base_type( parent_ )	{}

    sc_bv_out( const char* name_, this_type& parent_ )
		: base_type( name_, parent_ )	{}
};


template <int W>
class sc_bv_in : public sc_in<sc_bv_base>
{
public:
	typedef sc_in<sc_bv_base>                             base_type;
	typedef typename base_type::inout_if_type             inout_if_type;
	typedef typename base_type::inout_port_type           inout_port_type;
	typedef sc_bv_in<W>                                   this_type;

	sc_bv_in()
		: base_type(){}

    explicit sc_bv_in( const char* name_ )
		: base_type( name_ )	{}

    explicit sc_bv_in( const in_if_type& interface_ )
        : base_type( interface )        {}

    sc_bv_in( const char* name_, const in_if_type& interface_ )
		: base_type( name, interface_ )	{}

    explicit sc_bv_in( in_port_type& parent_ )
		: base_type( parent_ )	{}

    sc_bv_in( const char* name_, in_port_type& parent_ )
		: base_type( name_, parent_ )	{}

    explicit sc_bv_in( inout_port_type& parent_ )
		: base_type( parent_ ) {}

    sc_bv_in( const char* name_, inout_port_type& parent_ )
		: base_type( name_, parent_ ) { }

    sc_bv_in( this_type& parent_ )
		: base_type( parent_ )	{}

    sc_bv_in( const char* name_, this_type& parent_ )
		: base_type( name_, parent_ )	{}
};