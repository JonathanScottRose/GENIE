#pragma once

#include <unordered_map>
#include <functional>
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
	typedef std::function<void(sc_module&, sc_clock*)> BinderFunc;

	BinderFunc binder;
	std::string name;
};


struct ATCtrlSignalDef
{
	typedef std::function<void(sc_module&, sc_signal<bool>*)> BinderFunc;
	
	BinderFunc binder;
};


struct ATDataSignalDef
{
	typedef std::function<void(sc_module&, sc_bv_signal*)> BinderFunc;
	typedef std::function<sc_bv_signal*(void)> CreateFunc;

	BinderFunc binder;
	CreateFunc creator;
	std::string usertype;
	int width;
};


struct ATAddrSignalDef
{
	typedef std::function<void(sc_module&, sc_bv_signal*)> BinderFunc;
	typedef std::function<sc_bv_signal*(void)> CreateFunc;

	BinderFunc binder;
	CreateFunc creator;
	int width;
};


// Endpoint definition


struct ATEndpointDef
{
	enum Type
	{
		UNICAST,
		UNICAST_NATIVE,
		BROADCAST
	};

	ATEndpointDef(const std::string& name, int ep_id, Type type);
	~ATEndpointDef();

	std::string name;
	int ep_id;
	Type type;
};


// Interface definition

class ATInterfaceDef
{
public:
	typedef std::unordered_map<std::string, struct ATEndpointDef*> EndpointMap;
	typedef std::unordered_map<std::string, struct ATDataSignalDef*> DataSigMap;

	enum Type
	{
		CLOCK_SOURCE,
		CLOCK_SINK,
		SEND,
		RECV
	};

	ATInterfaceDef(const std::string& name, const std::string& clock, Type type, 
		ATEndpointDef::Type def_ep_type);
	~ATInterfaceDef();

	Type get_type() { return m_type; }
	const std::string& get_name() { return m_name; }
	const std::string& get_clock() { return m_clock; }
	ATEndpointDef::Type get_default_ep_type() { return m_def_ep_type; }

	// Definition
	void define_endpoint(const std::string& name, int addr, ATEndpointDef::Type type);

	void define_data_signal(
		const ATDataSignalDef::BinderFunc& binder, 
		const ATDataSignalDef::CreateFunc& creator,
		const std::string& usertype, int width);
	void define_header_signal(
		const ATDataSignalDef::BinderFunc& binder, 
		const ATDataSignalDef::CreateFunc& creator,
		const std::string& usertype, int width);
	void define_address_signal(
		const ATAddrSignalDef::BinderFunc& binder, 
		const ATAddrSignalDef::CreateFunc& creator, int width);
	void define_ep_signal(
		const ATAddrSignalDef::BinderFunc& binder,
		const ATAddrSignalDef::CreateFunc& creator, int width);
	void define_valid_signal(const ATCtrlSignalDef::BinderFunc& binder);
	void define_ready_signal(const ATCtrlSignalDef::BinderFunc& binder);
	void define_sop_signal(const ATCtrlSignalDef::BinderFunc& binder);
	void define_eop_signal(const ATCtrlSignalDef::BinderFunc& binder);

	// Signal query
	bool has_data();
	bool has_header();
	bool has_address();
	bool has_ep();
	bool has_valid();
	bool has_ready();
	bool has_sop();
	bool has_eop();

	int get_data_width() { return m_data_width; }
	int get_header_width() { return m_header_width; }

	bool has_composite_data() { return m_sigdefs_data.size() > 1; }
	bool has_composite_header() { return m_sigdefs_header.size() > 1; }

	// Retrieval
	ATCtrlSignalDef* get_valid_signal() { return m_sigdef_valid; }
	ATCtrlSignalDef* get_ready_signal() { return m_sigdef_ready; }
	ATCtrlSignalDef* get_sop_signal() { return m_sigdef_sop; }
	ATCtrlSignalDef* get_eop_signal() { return m_sigdef_eop; }
	ATAddrSignalDef* get_address_signal() { return m_sigdef_addr; }
	ATAddrSignalDef* get_ep_signal() { return m_sigdef_ep; }

	const DataSigMap& data_signals() { return m_sigdefs_data; }
	ATDataSignalDef* get_data_signal(const std::string& nm) { return m_sigdefs_data[nm]; }
	
	const DataSigMap& header_signals() { return m_sigdefs_header; }
	ATDataSignalDef* get_header_signal(const std::string& nm) { return m_sigdefs_header[nm]; }

	const EndpointMap& endpoints() { return m_ep_defs; }
	ATEndpointDef* get_endpoint(const std::string& name) { return m_ep_defs[name]; }

private:
	std::string m_name;
	std::string m_clock;
	Type m_type;
	ATEndpointDef::Type m_def_ep_type;

	EndpointMap m_ep_defs;

	ATCtrlSignalDef* m_sigdef_valid;
	ATCtrlSignalDef* m_sigdef_ready;
	ATCtrlSignalDef* m_sigdef_sop;
	ATCtrlSignalDef* m_sigdef_eop;
	DataSigMap m_sigdefs_data;
	DataSigMap m_sigdefs_header;
	ATAddrSignalDef* m_sigdef_addr;
	ATAddrSignalDef* m_sigdef_ep;

	int m_header_width;
	int m_data_width;
};


class ATLinkPointDef
{
public:
	ATLinkPointDef() { }
	ATLinkPointDef(const std::string& path)
	{
		std::stringstream strm(path);
		std::getline(strm, m_inst, '.');
		std::getline(strm, m_iface, '.');
		if (!std::getline(strm, m_ep, '.'))
			m_ep = "ep";
	}

	const std::string& get_inst() const { return m_inst; }
	const std::string& get_iface() const { return m_iface; }
	const std::string& get_ep() const { return m_ep; }

	std::string get_path()
	{
		std::string result(m_inst);
		return result.append(".").append(m_iface).append(".").append(m_ep);
	}

private:
	std::string m_inst;
	std::string m_iface;
	std::string m_ep;
};


// Link definition

struct ATLinkDef
{
	ATLinkPointDef src;
	ATLinkPointDef dest;
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
	typedef std::unordered_map<std::string, ATInterfaceDef*> IfaceMap;
	typedef std::unordered_map<std::string, ATClockSignalDef*> ClockMap;
	typedef std::function<sc_module*(const char*)> InsterFunc;

	ATComponentDef(const std::string& name, const InsterFunc& inster);
	~ATComponentDef();

	ATInterfaceDef* define_interface(
		const std::string& name, 
		const std::string& clock,
		ATInterfaceDef::Type type,
		ATEndpointDef::Type def_ep_type);

	void define_clock(
		const ATClockSignalDef::BinderFunc& binder,
		const std::string& name);

	InsterFunc get_inster() { return m_inster; }

	const IfaceMap& interfaces() { return m_iface_defs; }
	ATInterfaceDef* get_iface(const std::string& n) { return m_iface_defs[n]; }

	const ClockMap& clocks() { return m_clock_defs; }
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
	typedef std::unordered_map<std::string, ATComponentDef*> CompDefMap;
	typedef std::unordered_map<std::string, ATInstanceDef*> InstDefMap;
	typedef std::vector<ATLinkDef*> LinkDefVec;

	ATSpec();
	~ATSpec();

	ATComponentDef* define_component(const std::string& name, const ATComponentDef::InsterFunc& inster);
	ATInstanceDef* define_instance(const std::string& name, const std::string& component);
	ATLinkDef* define_link(const std::string& src, const std::string& dest);
	
	ATComponentDef* get_component_def(const std::string& name) { return m_comp_defs[name]; }
	ATInstanceDef* get_instance_def(const std::string& name) { return m_inst_defs[name]; }

	ATComponentDef* get_component_def_for_instance(const std::string& name);
	ATEndpointDef* get_endpoint_def_for_linkpoint(const ATLinkPointDef& lp_def);

	const CompDefMap& comp_defs() { return m_comp_defs; }
	const InstDefMap& inst_defs() { return m_inst_defs; }
	const LinkDefVec& link_defs() { return m_link_defs; }

	void validate_and_preprocess();

private:
	CompDefMap m_comp_defs;
	InstDefMap m_inst_defs;
	LinkDefVec m_link_defs;
};