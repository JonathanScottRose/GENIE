#pragma once

#include <vector>
#include <string>
#include <map>
#include "at_spec.h"
#include "at_netlist.h"


class ATManager
{
public:
	static ATManager* inst();

	ATSpec* get_spec() { return &m_spec; }
	void bind_clock(sc_clock& clk, const std::string& name);
	void build_system();
	sc_module* get_module(const std::string& name) { return m_modules[name]; }
	sc_clock* get_clock(const std::string& name) { return m_clocks[name]; }

private:
	typedef std::map<std::string, sc_module*> ModMap;
	typedef std::map<std::string, sc_clock*> ClockMap;

	ATManager();
	~ATManager();

	void build_netlist();

	void transform_netlist();
	void xform_arbs();
	void xform_bcasts();
	void xform_proto_match();

	ClockMap m_clocks;
	ModMap m_modules;
	ATNetlist m_netlist;
	ATSpec m_spec;
};
