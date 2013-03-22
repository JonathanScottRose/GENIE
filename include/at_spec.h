#pragma once

#include <map>
#include <systemc.h>
#include "sc_bv_signal.h"

// Forward declarations
class ATSpec;
struct ATClockSignalDef;
struct ATCtrlSignalDef;
struct ATDataSignalDef;
struct ATAddrSignalDef;
class ATInterfaceDef;
struct ATEndpointDef;
struct ATLinkDef;
struct ATInstanceDef;
class ATComponentDef;


// Signal definition

struct ATClockSignalDef
{
	typedef void (*BinderFunc)(sc_module&, sc_clock*);

	BinderFunc binder;
	std::string name;
};


struct ATCtrlSignalDef
{
	typedef void (*BinderFunc)(sc_module&, sc_signal<bool>*);
	
	BinderFunc binder;
};


struct ATDataSignalDef
{
	typedef void (*BinderFunc)(sc_module&, sc_bv_signal*);
	typedef sc_bv_signal* (*CreateFunc)();

	BinderFunc binder;
	CreateFunc creator;
	std::string usertype;
	int width;
};


struct ATAddrSignalDef
{
	typedef void (*BinderFunc)(sc_module&, sc_bv_signal*);
	typedef sc_bv_signal* (*CreateFunc)();

	BinderFunc binder;
	CreateFunc creator;
	int width;
};


// Interface definition

class ATInterfaceDef
{
public:
	typedef std::map<std::string, struct ATEndpointDef*> EndpointMap;
	typedef std::map<std::string, struct ATDataSignalDef*> DataSigMap;

	enum Type
	{
		CLOCK_SOURCE,
		CLOCK_SINK,
		SEND,
		RECV
	};

	ATInterfaceDef(const std::string& name, const std::string& clock, Type type);
	~ATInterfaceDef();

	Type get_type() { return m_type; }
	const std::string& get_name() { return m_name; }
	const std::string& get_clock() { return m_clock; }

	void define_endpoint(const std::string& name, int addr);

	void define_data_signal(ATDataSignalDef::BinderFunc binder, 
		ATDataSignalDef::CreateFunc creator,
		const std::string& usertype, int width);
	void define_header_signal(ATDataSignalDef::BinderFunc binder, 
		ATDataSignalDef::CreateFunc creator,
		const std::string& usertype, int width);
	void define_address_signal(ATAddrSignalDef::BinderFunc binder, 
		ATAddrSignalDef::CreateFunc creator, int width);
	void define_valid_signal(ATCtrlSignalDef::BinderFunc binder);
	void define_ready_signal(ATCtrlSignalDef::BinderFunc binder);
	void define_sop_signal(ATCtrlSignalDef::BinderFunc binder);
	void define_eop_signal(ATCtrlSignalDef::BinderFunc binder);

	bool has_data();
	bool has_header();
	bool has_address();
	bool has_valid();
	bool has_ready();
	bool has_sop();
	bool has_eop();

	int get_data_width() { return m_data_width; }
	int get_header_width() { return m_header_width; }

	bool has_composite_data() { return m_sigdefs_data.size() > 1; }
	bool has_composite_header() { return m_sigdefs_header.size() > 1; }

	ATCtrlSignalDef* get_valid_signal() { return m_sigdef_valid; }
	ATCtrlSignalDef* get_ready_signal() { return m_sigdef_ready; }
	ATCtrlSignalDef* get_sop_signal() { return m_sigdef_sop; }
	ATCtrlSignalDef* get_eop_signal() { return m_sigdef_eop; }
	ATAddrSignalDef* get_address_signal() { return m_sigdef_addr; }

	ATDataSignalDef* get_data_signal(const std::string& nm) { return m_sigdefs_data[nm]; }
	DataSigMap::iterator data_sigs_begin() { return m_sigdefs_data.begin(); }
	DataSigMap::iterator data_sigs_end() { return m_sigdefs_data.end(); }
	ATDataSignalDef* get_header_signal(const std::string& nm) { return m_sigdefs_header[nm]; }
	DataSigMap::iterator header_sigs_begin() { return m_sigdefs_header.begin(); }
	DataSigMap::iterator header_sigs_end() { return m_sigdefs_header.end(); }

private:
	std::string m_name;
	std::string m_clock;

	Type m_type;
	EndpointMap m_ep_defs;
	ATCtrlSignalDef* m_sigdef_valid;
	ATCtrlSignalDef* m_sigdef_ready;
	ATCtrlSignalDef* m_sigdef_sop;
	ATCtrlSignalDef* m_sigdef_eop;
	DataSigMap m_sigdefs_data;
	DataSigMap m_sigdefs_header;
	ATAddrSignalDef* m_sigdef_addr;

	int m_header_width;
	int m_data_width;
};


// Endpoint definition

struct ATEndpointDef
{
	ATEndpointDef(const std::string& name, int value);
	~ATEndpointDef();

	std::string name;
	int value;
};


// Link definition

struct ATLinkDef
{
	std::string src;
	std::string dest;
};


// Component instance definition

struct ATInstanceDef
{
	std::string inst_name;
	std::string comp_name;
};


// Component definition

class ATComponentDef
{
public:
	typedef std::map<std::string, ATInterfaceDef*> IfaceMap;
	typedef sc_module* (*InsterFunc)(const char*);
	typedef std::map<std::string, ATClockSignalDef*> ClockMap;

	ATComponentDef(const std::string& name, InsterFunc inster);
	~ATComponentDef();

	ATInterfaceDef* define_interface(const std::string& name, 
		const std::string& clock,
		ATInterfaceDef::Type type);

	void define_clock(ATClockSignalDef::BinderFunc binder,
		const std::string& name);

	InsterFunc get_inster() { return m_inster; }

	IfaceMap::iterator iface_begin() { return m_iface_defs.begin(); }
	IfaceMap::iterator iface_end() { return m_iface_defs.end(); }
	ATInterfaceDef* get_iface(const std::string& n) { return m_iface_defs[n]; }

	ClockMap::iterator clock_begin() { return m_clock_defs.begin(); }
	ClockMap::iterator clock_end() { return m_clock_defs.end(); }
	ATClockSignalDef* get_clock(const std::string& n) { return m_clock_defs[n]; }

private:
	std::string m_name;
	InsterFunc m_inster;
	IfaceMap m_iface_defs;
	ClockMap m_clock_defs;
};


// The spec

class ATSpec
{
public:
	typedef std::map<std::string, ATComponentDef*> CompDefMap;
	typedef std::map<std::string, ATInstanceDef*> InstDefMap;
	typedef std::vector<ATLinkDef*> LinkDefVec;

	ATSpec();
	~ATSpec();

	ATComponentDef* define_component(const std::string& name, ATComponentDef::InsterFunc inster);
	ATInstanceDef* define_instance(const std::string& name, const std::string& component);
	ATLinkDef* define_link(const std::string& src, const std::string& dest);
	
	ATComponentDef* get_component_def(const std::string& name) { return m_comp_defs[name]; }
	ATInstanceDef* get_instance_def(const std::string& name) { return m_inst_defs[name]; }

	const CompDefMap& comp_defs() { return m_comp_defs; }
	const InstDefMap& inst_defs() { return m_inst_defs; }
	const LinkDefVec& link_defs() { return m_link_defs; }

private:
	CompDefMap m_comp_defs;
	InstDefMap m_inst_defs;
	LinkDefVec m_link_defs;
};