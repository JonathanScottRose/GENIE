#include "at_spec.h"
#include "at_util.h"

//
// Spec
//

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
	link->src = ATLinkPointDef(src);
	link->dest = ATLinkPointDef(dest);
	link->src_bind_pos = m_bind_positions[link->src.get_path()]++;
	link->dest_bind_pos = m_bind_positions[link->dest.get_path()]++;

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


ATComponentDef* ATSpec::get_component_def_for_instance(const std::string& name)
{
	const std::string& cname = get_instance_def(name)->comp_name;
	return get_component_def(cname);
}


ATEndpointDef* ATSpec::get_endpoint_def_for_linkpoint(const ATLinkPointDef& lp_def)
{
	auto compdef = get_component_def_for_instance(lp_def.get_inst());
	auto ifacedef = compdef->get_iface(lp_def.get_iface());
	return ifacedef->get_endpoint(lp_def.get_ep());
}


void ATSpec::validate_and_preprocess()
{
	// For now, just create default endpoints for interfaces
	for (auto i : m_comp_defs)
	{
		for (auto j : i.second->interfaces())
		{
			ATInterfaceDef* iface_def = j.second;

			if (iface_def->endpoints().empty())
			{
				iface_def->define_endpoint("ep", 0, iface_def->get_default_ep_type());				
			}
		}
	}
}


//
// ComponentDef
//


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
												 ATInterfaceDef::Type type,
												 ATEndpointDef::Type def_ep_type)
{
	ATInterfaceDef* def = new ATInterfaceDef(name, clock, type, def_ep_type);
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


//
// InterfaceDef
//


ATInterfaceDef::ATInterfaceDef(const std::string &name, const std::string& clock,
							   ATInterfaceDef::Type type, ATEndpointDef::Type def_ep_type)
: m_name(name), m_clock(clock), m_type(type), m_header_width(0), m_data_width(0),
m_def_ep_type(def_ep_type)
{
	m_sigdef_addr = nullptr;
	m_sigdef_valid = nullptr;
	m_sigdef_ready = nullptr;
	m_sigdef_sop = nullptr;
	m_sigdef_eop = nullptr;
	m_sigdef_ep = nullptr;
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


void ATInterfaceDef::define_endpoint(const std::string &name, int addr, ATEndpointDef::Type type)
{
	ATEndpointDef* ep = new ATEndpointDef(name, addr, type);
	m_ep_defs[name] = ep;
}


//
// EndpointDef
//


ATEndpointDef::ATEndpointDef(const std::string &name, int ep_id, Type type)
	: name(name), ep_id(ep_id), type(type)
{
}


ATEndpointDef::~ATEndpointDef()
{
}


