#pragma once

#include "p2p.h"

namespace ct
{
namespace P2P
{
	class SplitNode : public Node
	{
	public:
		typedef std::vector<DataPort*> DestVec;
		typedef std::unordered_map<int, DestVec> RouteMap;

		SplitNode(const std::string& name, const Protocol& proto, int n_outputs);
		~SplitNode();

		PROP_GET(n_outputs, int, m_n_outputs);
		PROP_GET(proto, const Protocol&, m_proto);

		DataPort* get_inport();
		DataPort* get_outport(int i);
		ClockResetPort* get_clock_port();
		ClockResetPort* get_reset_port();

		void register_flow(Flow* flow, DataPort* port);
	
	protected:
		Protocol m_proto;
		RouteMap m_route_map;	
		int m_n_outputs;
	};
}
}