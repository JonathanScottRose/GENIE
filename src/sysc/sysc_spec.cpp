#include "common.h"
#include "spec.h"
#include "static_init.h"
#include "sysc/sysc_spec.h"

using namespace ct;
using namespace ct::SysC;

namespace
{
	Spec::Component* s_cur_comp;
	Spec::DataInterface* s_cur_iface;

	Util::InitShutdown s_ctor_dtor([]
	{
		s_cur_comp = nullptr;
		s_cur_iface = nullptr;
	}, []
	{
	});

	void define_interface(const std::string& name, const std::string& clock, Spec::Interface::Type type)
	{
		s_cur_iface = new Spec::DataInterface(name, type);
		s_cur_iface->set_clock(clock);
		s_cur_comp->add_interface(s_cur_iface);		
	}

	void define_linkpoint(const std::string& name, Spec::Linkpoint::Type type, int encoding)
	{
		Spec::Linkpoint* lp = new Spec::Linkpoint(name, type);
		LinkpointInfo* lp_info = new LinkpointInfo;
		lp_info->encoding = encoding;
		lp->set_encoding(lp_info);
	}

	void define_bool_signal(Spec::Signal::Type type, const BoolBinder& binder)
	{
		Spec::Signal* sig = new Spec::Signal(type, 1);
		BoolSignalInfo* info = new BoolSignalInfo;
		info->binder = binder;
		sig->set_impl(info);
		s_cur_iface->add_signal(sig);
	}

	void define_vec_signal(Spec::Signal::Type type, const VecBinder& binder, int width,
		const std::string& usertype = "")
	{
		Spec::Signal* sig = new Spec::Signal(type, width);
		VecSignalInfo* info = new VecSignalInfo;
		info->binder = binder;
		sig->set_impl(info);
		sig->set_subtype(usertype);
		s_cur_iface->add_signal(sig);
	}
}

void Macros::define_component(const std::string& name, const InsterFunc& inster)
{
	s_cur_comp = new Spec::Component(name);
	CompInfo* compinfo = new CompInfo;
	compinfo->inster = inster;
	Spec::define_component(s_cur_comp);
}

void Macros::define_clock(const std::string& name, const ClockBinder& binder)
{
	Spec::Interface* iface = new Spec::ClockResetInterface(name, Spec::Interface::CLOCK_SINK);
	Spec::Signal* sig = new Spec::Signal(Spec::Signal::CLOCK, 1);
	ClockSignalInfo* info = new ClockSignalInfo;
	info->binder = binder;
	sig->set_impl(info);
	iface->add_signal(sig);
}

void Macros::define_send_interface(const std::string& name, const std::string& clock)
{
	define_interface(name, clock, Spec::Interface::SEND);
}

void Macros::define_recv_interface(const std::string& name, const std::string& clock)
{
	define_interface(name, clock, Spec::Interface::RECV);
}

void Macros::define_ucast_linkpoint(const std::string& name, int encoding)
{
	define_linkpoint(name, Spec::Linkpoint::UNICAST, encoding);
}
	
void Macros::define_bcast_linkpoint(const std::string& name, int encoding)
{
	define_linkpoint(name, Spec::Linkpoint::BROADCAST, encoding);
}

void Macros::define_valid_signal(const BoolBinder& binder)
{
	define_bool_signal(Spec::Signal::VALID, binder);	
}

void Macros::define_ready_signal(const BoolBinder& binder)
{
	define_bool_signal(Spec::Signal::READY, binder);	
}

void Macros::define_sop_signal(const BoolBinder& binder)
{
	define_bool_signal(Spec::Signal::SOP, binder);	
}

void Macros::define_eop_signal(const BoolBinder& binder)
{
	define_bool_signal(Spec::Signal::EOP, binder);	
}

void Macros::define_data_signal(const VecBinder& binder, const std::string& name, int width)
{
	define_vec_signal(Spec::Signal::DATA, binder, width, name);
}

void Macros::define_data_signal(const VecBinder& binder, int width)
{
	define_vec_signal(Spec::Signal::DATA, binder, width);
}

void Macros::define_header_signal(const VecBinder& binder, const std::string& name, int width)
{
	define_vec_signal(Spec::Signal::HEADER, binder, width, name);
}

void Macros::define_header_signal(const VecBinder& binder, int width)
{
	define_vec_signal(Spec::Signal::HEADER, binder, width);
}

void Macros::define_lp_id_signal(const VecBinder& binder, int width)
{
	define_vec_signal(Spec::Signal::LP_ID, binder, width);
}

void Macros::define_link_id_signal(const VecBinder& binder, int width)
{
	define_vec_signal(Spec::Signal::LINK_ID, binder, width);
}