#include "at_spec.h"
#include "at_util.h"

ATSpec::ATSpec()
{
}


ATSpec::~ATSpec()
{
	at_delete_all_2(m_comp_defs);
	at_delete_all_2(m_inst_defs);
	at_delete_all(m_link_defs);
}


ATComponentDef* ATSpec::define_component(const std::string& name, 
										 const ATComponentDef::InsterFunc& inster)
{
	ATComponentDef* comp = new ATComponentDef(name, inster);
	m_comp_defs[name] = comp;
	return comp;
}


ATLinkDef* ATSpec::define_link(const std::string &src, const std::string &dest)
{
	ATLinkDef* link = new ATLinkDef;
	link->src = src;
	link->dest = dest;
	m_link_defs.push_back(link);
	return link;
}


ATInstanceDef* ATSpec::define_instance(const std::string &name,
									   const std::string &component)
{
	ATInstanceDef* inst = new ATInstanceDef;
	inst->comp_name = component;
	inst->inst_name = name;
	m_inst_defs[name] = inst;
	return inst;
}



ATComponentDef::ATComponentDef(const std::string &name, 
							   const ATComponentDef::InsterFunc& inster)
: m_name(name), m_inster(inster)
{
}


ATComponentDef::~ATComponentDef()
{
	at_delete_all_2(m_iface_defs);
}


ATInterfaceDef* ATComponentDef::define_interface(const std::string& name, 
												 const std::string& clock,
												 ATInterfaceDef::Type type)
{
	ATInterfaceDef* def = new ATInterfaceDef(name, clock, type);
	m_iface_defs[name] = def;
	return def;
}


void ATComponentDef::define_clock(const ATClockSignalDef::BinderFunc& binder, 
								  const std::string &name)
{
	ATClockSignalDef* def = new ATClockSignalDef;
	def->binder = binder;
	def->name = name;
	m_clock_defs[name] = def;
}


ATInterfaceDef::ATInterfaceDef(const std::string &name, const std::string& clock,
							   ATInterfaceDef::Type type)
: m_name(name), m_clock(clock), m_type(type), m_header_width(0), m_data_width(0)
{
	m_sigdef_addr = 0;
	m_sigdef_valid = 0;
	m_sigdef_ready = 0;
	m_sigdef_sop = 0;
	m_sigdef_eop = 0;
}


ATInterfaceDef::~ATInterfaceDef()
{
	at_delete_all_2(m_ep_defs);
	at_delete_all_2(m_sigdefs_data);
	at_delete_all_2(m_sigdefs_header);
	if (m_sigdef_valid) delete m_sigdef_valid;
	if (m_sigdef_ready) delete m_sigdef_ready;
	if (m_sigdef_sop) delete m_sigdef_sop;
	if (m_sigdef_eop) delete m_sigdef_eop;
	if (m_sigdef_addr) delete m_sigdef_addr;
	if (m_sigdef_ep) delete m_sigdef_ep;
}


void ATInterfaceDef::define_address_signal(const ATAddrSignalDef::BinderFunc& binder,
										   const ATAddrSignalDef::CreateFunc& creator, int width)
{
	ATAddrSignalDef* def = new ATAddrSignalDef;
	def->binder = binder;
	def->creator = creator;
	def->width = width;
	m_sigdef_addr = def;
}


void ATInterfaceDef::define_ep_signal(const ATAddrSignalDef::BinderFunc& binder,
									  const ATAddrSignalDef::CreateFunc& creator, int width)
{
	ATAddrSignalDef* def = new ATAddrSignalDef;
	def->binder = binder;
	def->creator = creator;
	def->width = width;
	m_sigdef_ep = def;
}


void ATInterfaceDef::define_data_signal(const ATDataSignalDef::BinderFunc& binder,
										const ATDataSignalDef::CreateFunc& creator,
										const std::string& usertype, int width)
{
	ATDataSignalDef* def = new ATDataSignalDef;
	def->binder = binder;
	def->creator = creator;
	def->usertype = usertype;
	def->width = width;
	m_sigdefs_data[usertype] = def;
	m_data_width += width;
}


void ATInterfaceDef::define_header_signal(const ATDataSignalDef::BinderFunc& binder,
										  const ATDataSignalDef::CreateFunc& creator,
										  const std::string &usertype, int width)
{
	ATDataSignalDef* def = new ATDataSignalDef;
	def->binder = binder;
	def->creator = creator;
	def->usertype = usertype;
	def->width = width;
	m_sigdefs_header[usertype] = def;
	m_header_width += width;
}


void ATInterfaceDef::define_ready_signal(const ATCtrlSignalDef::BinderFunc& binder)
{
	ATCtrlSignalDef* def = new ATCtrlSignalDef;
	def->binder = binder;
	m_sigdef_ready = def;
}


void ATInterfaceDef::define_eop_signal(const ATCtrlSignalDef::BinderFunc& binder)
{
	ATCtrlSignalDef* def = new ATCtrlSignalDef;
	def->binder = binder;
	m_sigdef_eop = def;
}


void ATInterfaceDef::define_sop_signal(const ATCtrlSignalDef::BinderFunc& binder)
{
	ATCtrlSignalDef* def = new ATCtrlSignalDef;
	def->binder = binder;
	m_sigdef_sop = def;
}


void ATInterfaceDef::define_valid_signal(const ATCtrlSignalDef::BinderFunc& binder)
{
	ATCtrlSignalDef* def = new ATCtrlSignalDef;
	def->binder = binder;
	m_sigdef_valid = def;
}


bool ATInterfaceDef::has_address() { return m_sigdef_addr != 0; }
bool ATInterfaceDef::has_ep() { return m_sigdef_ep!= 0; }
bool ATInterfaceDef::has_data() { return !m_sigdefs_data.empty(); }
bool ATInterfaceDef::has_eop() { return m_sigdef_eop != 0; }
bool ATInterfaceDef::has_header() { return !m_sigdefs_header.empty(); }
bool ATInterfaceDef::has_ready() { return m_sigdef_ready != 0; }
bool ATInterfaceDef::has_sop() { return m_sigdef_sop != 0; }
bool ATInterfaceDef::has_valid() { return m_sigdef_valid != 0; }


void ATInterfaceDef::define_endpoint(const std::string &name, int addr)
{
	ATEndpointDef* ep = new ATEndpointDef(name, addr);
	m_ep_defs[name] = ep;
}


ATEndpointDef::ATEndpointDef(const std::string &name, int value)
: name(name), value(value) { }


ATEndpointDef::~ATEndpointDef() { }