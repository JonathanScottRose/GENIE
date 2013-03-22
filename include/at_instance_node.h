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

	ATNetInPort* get_port_for_recv(const std::string& name);
	ATNetOutPort* get_port_for_send(const std::string& name);

	sc_module* get_impl() { return m_impl; }

private:
	typedef std::map<std::string, ATNetInPort*> RecvIfaceMap;
	typedef std::map<std::string, ATNetOutPort*> SendIfaceMap;

	sc_module* m_impl;
	ATInstanceDef* m_instance;
	RecvIfaceMap m_recv_map;
	SendIfaceMap m_send_map;
};
