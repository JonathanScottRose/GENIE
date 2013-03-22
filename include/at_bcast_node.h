#pragma once

#include "at_netlist.h"
#include "at_protocol.h"

class ATBcastNode : public ATNetNode
{
public:
	ATBcastNode(int n_outputs, const ATLinkProtocol& in_proto, const ATLinkProtocol& out_proto,
		const std::string& clock);
	~ATBcastNode();

	void instantiate();
	void set_addr(int port, int addr);

protected:
	int m_n_outputs;
	ATLinkProtocol m_in_proto;
	ATLinkProtocol m_out_proto;
	std::string m_clock;
	std::vector<int> m_addr_map;
};

