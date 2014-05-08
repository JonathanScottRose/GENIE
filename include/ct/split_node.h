#pragma once

#include "ct/p2p.h"

namespace ct
{
namespace P2P
{
	class SplitNode : public Node
	{
	public:
		typedef std::vector<int> DestVec;
		typedef std::unordered_map<int, DestVec> RouteMap;

		SplitNode(const std::string& name, int n_outputs);

		PROP_GET(n_outputs, int, m_n_outputs);

		Port* get_inport();
		Port* get_outport(int i);
		ClockResetPort* get_clock_port();
		ClockResetPort* get_reset_port();
		Port* get_free_outport();

		Protocol& get_proto();
		int get_n_flows();
		int get_flow_id_width();
		const DestVec& get_dests_for_flow(int flow_id);
		const Port::Flows& get_flows();
		int get_idx_for_outport(Port* port);
		
		PortList trace(Port* in, Flow* f);
		Port* rtrace(Port* port, Flow* flow);
		void configure_1();
		void configure_2();
		void carry_fields(const FieldSet& set);
	
	protected:
		void register_flow(Flow* flow, int outport_idx);

		RouteMap m_route_map;	
		int m_n_outputs;
	};
}
}