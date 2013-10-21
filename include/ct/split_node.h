#pragma once

#include "p2p.h"

namespace ct
{
namespace P2P
{
	class SplitNode : public Node
	{
	public:
		typedef std::vector<int> DestVec;
		typedef std::unordered_map<int, DestVec> RouteMap;

		SplitNode(const std::string& name, const Protocol& proto, int n_outputs);
		~SplitNode();

		PROP_GET(n_outputs, int, m_n_outputs);
		PROP_GET(proto, const Protocol&, m_proto);

		DataPort* get_inport();
		DataPort* get_outport(int i);
		ClockResetPort* get_clock_port();
		ClockResetPort* get_reset_port();

		void configure();

		void register_flow(Flow* flow, int outport_idx);
		int get_n_flows();
		int get_flow_id_width();
		const DestVec& get_dests_for_flow(int flow_id);
		const DataPort::Flows& get_flows();
		int get_idx_for_outport(Port* port);
	
	protected:
		Protocol m_proto;
		RouteMap m_route_map;	
		int m_n_outputs;
	};
}
}