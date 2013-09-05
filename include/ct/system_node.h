#pragma once

#include "p2p.h"
#include "spec.h"

namespace ct
{
namespace P2P
{
	class SystemNode : public Node
	{
	public:
		typedef std::unordered_map<std::string, Node*> NodeMap;
		typedef std::vector<Conn*> ConnVec;
		typedef std::unordered_map<int, Flow*> Flows;

		SystemNode();
		~SystemNode();

		PROP_GET_SET(spec, Spec::System*, m_sys_spec);

		const NodeMap& nodes() { return m_nodes; }
		Node* get_node(const std::string& name) { return m_nodes[name]; }
		void add_node(Node* node);
		void remove_node(Node* node);

		const ConnVec& conns() { return m_conns; }
		void add_conn(Conn* conn);
		void remove_conn(Conn* conn);

		const Flows& flows() { return m_flows; }
		Flow* get_flow(int id) { return m_flows[id]; }
		void add_flow(Flow* flow);

		void connect_ports(DataPort* src, DataPort* dest);
		void disconnect_ports(DataPort* src, DataPort* dest);

		void dump_graph();

	protected:
		Spec::System* m_sys_spec;
		NodeMap m_nodes;
		Flows m_flows;
		ConnVec m_conns;
	};
}
}
