#pragma once

#include <systemc.h>
#include <functional>
#include "ct/sc_bv_signal.h"
#include "ct/sc_fwd_port.h"

namespace ct
{
namespace SysC
{

typedef std::function<sc_module*(const char*)> InsterFunc;
typedef std::function<void(sc_module&, sc_clock*)> ClockBinder;
typedef std::function<void(sc_module&, sc_bv_signal*)> VecBinder;
typedef std::function<void(sc_module&, sc_signal<bool>*)> BoolBinder;

namespace Macros
{

void define_component(const std::string& name, const InsterFunc& inster);
void define_clock(const std::string& name, const ClockBinder& binder);
void define_send_interface(const std::string& name, const std::string& clock);
void define_recv_interface(const std::string& name, const std::string& clock);
void define_ucast_linkpoint(const std::string& name, int encoding);
void define_bcast_linkpoint(const std::string& name, int encoding);
void define_valid_signal(const BoolBinder&);
void define_ready_signal(const BoolBinder&);
void define_sop_signal(const BoolBinder&);
void define_eop_signal(const BoolBinder&);
void define_data_signal(const VecBinder&, const std::string& name, int width);
void define_data_signal(const VecBinder&, int width);
void define_header_signal(const VecBinder&, const std::string& name, int width);
void define_header_signal(const VecBinder&, int width);
void define_lp_id_signal(const VecBinder&, int width);
void define_link_id_signal(const VecBinder&, int width);


namespace
{
	template <int W>
	int _psize (sc_out<sc_bv<W> > * port)
	{
		return W;
	}

	template <int W>
	int _psize (sc_in<sc_bv<W> > * port)
	{
		return W;
	}

	template <int W>
	void _bind (sc_out<sc_bv<W> > * port, sc_signal_inout_if<sc_bv_base>* sink)
	{
		sc_fwd_out* fwd = new sc_fwd_out<W> (sink);
		(*port)(*fwd);
	}

	template <int W>
	void _bind (sc_in<sc_bv<W> > * port, sc_signal_in_if<sc_bv_base>* src)
	{
		sc_fwd_in* fwd = new sc_fwd_in<W> (src);
		(*port)(*fwd);
	}
}

#define CT_COMPONENT(x) \
	namespace \
	{ \
		using namespace ct; \
		using namespace ct::SysC; \
		typedef x _comptype; \
		class _initializer \
		{ \
		public: \
			_initializer () \
			{ \
				Macros::define_component(#x, \
					[](const char* nm) { return new _comptype(nm); } \
				); \

#define CT_CLOCK(x,n) \
				Macros::define_clock(#n, \
					[](sc_module& module, sc_clock* sig) \
					{ \
						_comptype& cmodule = static_cast<_comptype&>(module); \
						cmodule.##x(*sig); \
					} \
				); \

#define CT_SEND_INTERFACE(x,clk) \
				Macros::define_send_interface(#x, #clk);

#define CT_RECV_INTERFACE(x,clk) \
				Macros::define_recv_interface(#x, #clk);
				
#define CT_LINKPOINT(n,a,t) \
				Macros::define_##t##_linkpoint(#n,a);

#define CT_BOOL(p,t) \
				Macros::define_##t##_signal( \
					[](sc_module& module, sc_signal<bool>* sig) \
					{ \
						_comptype& cmodule = static_cast<_comptype&>(module); \
						cmodule.##p##(*sig); \
					} \
				);

#define CT_VECONE(t,n,w) Macros::define_##t##_signal( t##_##n::bind_func, w)
#define CT_VECTWO(t,n,w) Macros::define_##t##_signal( t##_##n::bind_func, #n, w)

#define CT_VEC(p,t,n,f) \
				struct t##_##n { \
				static void bind_func(sc_module& module, sc_bv_signal* sig) \
				{ \
					_comptype& cmodule = static_cast<_comptype&>(module); \
					_bind(&cmodule.##p##, sig); \
				} \
				}; \
				##f(t,n,_psize(&((_comptype*)0)->##p##));

#define CT_VALID(x) CT_BOOL(x,valid)
#define CT_READY(x) CT_BOOL(x,ready)
#define CT_SOP(x) CT_BOOL(x,sop)
#define CT_EOP(x) CT_BOOL(x,eop)
#define CT_DATA(x) CT_VEC(x,data,,##CT_VECTWO)
#define CT_DATA_BUNDLE(x,n) CT_VEC(x,data,n,##CT_VECTWO)
#define CT_HEADER_BUNDLE(x,n) CT_VEC(x,header,n,##CT_VECTWO)
#define CT_HEADER(x) CT_VEC(x,header,,##CT_VECTWO)
#define CT_LINK_ID(x) CT_VEC(x,link_id,,##CT_VECONE)
#define CT_LPID(x) CT_VEC(x,lp_id,,##CT_VECONE)

#define CT_ENDCOMPONENT \
			} \
			~_initializer() \
			{ \
			} \
		}; \
		initializer _initializer_inst; \
	}


}
}
}