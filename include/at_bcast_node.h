#pragma once

#include "at_netlist.h"
#include "at_protocol.h"

class ATBcastNode : public ATNetNode
{
public:
	ATBcastNode(ATNet* net, const ATLinkProtocol& proto, const std::string& clock);
	~ATBcastNode();

	void instantiate();
	ATNetInPort* get_bcast_input();
	
protected:
	typedef std::vector<ATNetOutPort*> DestVec;
	typedef std::unordered_map<int, DestVec> RouteMap;

	ATLinkProtocol m_proto;
	RouteMap m_route_map;
	std::string m_clock;
};

