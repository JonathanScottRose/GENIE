#pragma once
#include "at_manager.h"
#include "at_spec.h"


namespace _at_macros
{
	namespace _at_send
	{
		template <int W>
		static int _psize (sc_out<sc_bv<W> > * port)
		{
			return W;
		}

		template <int W>
		static void _bind_internal (sc_out<sc_bv<W> > * port, sc_bv_signal* sig)
		{

			sc_signal<sc_bv<W> >* csig = reinterpret_cast<sc_signal<sc_bv<W> >* > (sig);
			(*port)(*csig);
		}

		template <int W>
		static sc_bv_signal* _create_internal(sc_out<sc_bv<W> > * port)
		{
			return (sc_bv_signal*) new sc_signal<sc_bv<W> > ();
		}
	}
	
	namespace _at_recv
	{
		template <int W>
		static int _psize (sc_in<sc_bv<W> > * port)
		{
			return W;
		}

		template <int W>
		static void _bind_internal (sc_in<sc_bv<W> > * port, sc_bv_signal* sig)
		{

			sc_signal<sc_bv<W> >* csig = reinterpret_cast<sc_signal<sc_bv<W> >* > (sig);
			(*port)(*csig);
		}

		template <int W>
		static sc_bv_signal* _create_internal(sc_in<sc_bv<W> > * port)
		{
			return (sc_bv_signal*) new sc_signal<sc_bv<W> > ();
		}
	}
}

#define AT_COMPONENT(x) \
	namespace _at_macros_##x \
	{ \
		using namespace _at_macros; \
		typedef x _comptype; \
		class _initializer \
		{ \
		public: \
			_initializer () \
			{ \
				ATSpec* spec = ATManager::inst()->get_spec(); \
				ATComponentDef* comp = spec->define_component(#x, \
					[](const char* nm) { return new _comptype(nm); } \
				); \
				ATInterfaceDef* iface; \
				std::string cur_clock; \
				{

#define AT_CLOCK(x,n) \
				} \
				comp->define_clock( \
					[](sc_module& module, sc_clock* sig) \
					{ \
						_comptype& cmodule = static_cast<_comptype&>(module); \
						cmodule.##x(*sig); \
					}, \
					#n \
				); \
				cur_clock = #n; \
				{ \

#define AT_SEND_INTERFACE(x) \
				} \
				{ \
					using namespace _at_send; \
					iface = comp->define_interface(#x, cur_clock, ATInterfaceDef::SEND, ATEndpointDef::UNICAST);

#define AT_RECV_INTERFACE(x) \
				} \
				{ \
					using namespace _at_recv; \
					iface = comp->define_interface(#x, cur_clock, ATInterfaceDef::RECV, ATEndpointDef::UNICAST);

#define AT_ENDPOINT(n,a,t) \
					iface->define_endpoint(#n,a,ATEndpointDef::##t);

#define _AT_BOOL(p,t) \
					iface->define_##t##_signal( \
						[](sc_module& module, sc_signal<bool>* sig) \
						{ \
							_comptype& cmodule = static_cast<_comptype&>(module); \
							cmodule.##p##(*sig); \
						} \
					);

#define _AT_VECONE(t,n,w) iface->define_##t##_signal( t##_##n::bind_func, t##_##n::create_func, w)
#define _AT_VECTWO(t,n,w) iface->define_##t##_signal( t##_##n::bind_func, t##_##n::create_func, #n, w)

#define _AT_VEC(p,t,n,f) \
					struct t##_##n { \
					static void bind_func(sc_module& module, sc_bv_signal* sig) \
					{ \
						_comptype& cmodule = static_cast<_comptype&>(module); \
						_bind_internal(&cmodule.##p##, sig); \
					} \
					static sc_bv_signal* create_func() \
					{ \
						_comptype* cmodule = (_comptype*)0; \
						return _create_internal(&cmodule->##p##); \
					} \
					}; \
					##f(t,n,_psize(&((_comptype*)0)->##p##));

#define AT_VALID(x) _AT_BOOL(x,valid)
#define AT_READY(x) _AT_BOOL(x,ready)
#define AT_SOP(x) _AT_BOOL(x,sop)
#define AT_EOP(x) _AT_BOOL(x,eop)
#define AT_DATA(x) _AT_VEC(x,data,,##_AT_VECTWO)
#define AT_DATA_BUNDLE(x,n) _AT_VEC(x,data,n,##_AT_VECTWO)
#define AT_HEADER(x,n) _AT_VEC(x,header,n,##_AT_VECTWO)
#define AT_ADDRESS(x) _AT_VEC(x,address,,##_AT_VECONE)
#define AT_EPID(x) _AT_VEC(x,ep,,##_AT_VECONE)

#define AT_ENDCOMPONENT \
				} \
			} \
			~_initializer() \
			{ \
			} \
		}; \
		static _initializer _initializer_inst; \
	}
