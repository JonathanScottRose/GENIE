#pragma once

#include "at_netlist.h"
#include "at_spec.h"

class ATInstanceNode : public ATNetNode
{
public:
	ATInstanceNode(ATInstanceDef* def);
	~ATInstanceNode();

	ATInstanceDef* get_instance_def() { return m_instance; }
	void instantiate();

	int get_addr() { return m_addr; }
	void set_addr(int addr) { m_addr = addr; }

	sc_module* get_impl() { return m_impl; }
	
private:
	ATInstanceDef* m_instance;
	sc_module* m_impl;
	int m_addr;
};
