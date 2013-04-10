#pragma once

#include "at_netlist.h"
#include "at_protocol.h"

class ATArbNode : public ATNetNode
{
public:
	ATArbNode(const std::vector<ATNet*>& nets, const ATLinkProtocol& proto,
		const std::string& clock);
	~ATArbNode();

	void instantiate();
	ATNetOutPort* get_arb_outport();

protected:
	ATLinkProtocol m_proto;
	std::string m_clock;
};
