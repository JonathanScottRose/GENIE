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
	ATNetlist* get_netlist() { return &m_netlist; }

	void bind_clock(sc_clock& clk, const std::string& name);
	void build_system();

	sc_clock* get_clock(const std::string& name) { return m_clocks[name]; }

private:
	typedef std::map<std::string, sc_clock*> ClockMap;

	ATManager();
	~ATManager();

	void init_netlist();
	void create_arb_bcast();

	ClockMap m_clocks;
	ATNetlist m_netlist;
	ATSpec m_spec;
};
