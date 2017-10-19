#pragma once

#include <functional>
#include "graph.h"
#include "network.h"
#include "address.h"

namespace genie
{
namespace impl
{
	class Port;
	class PortRS;
	class NodeSystem;
	class LinkRSLogical;

namespace flow
{
	using TransmissionID = unsigned;

	class DomainRS
	{
	public:
		static constexpr unsigned INVALID = std::numeric_limits<unsigned>::max();
		using Links = std::vector<LinkID>;
		using Transmissions = std::vector<TransmissionID>;

		DomainRS();
		DomainRS(const DomainRS&) = default;
		~DomainRS() = default;

		PROP_GET_SET(id, unsigned, m_id);
		PROP_GET_SET(is_manual, bool, m_is_manual);
		PROP_GET_SET(opt_disabled, bool, m_disable_opt);
		PROP_GET_SET(name, const std::string&, m_name);

		void add_link(LinkID);
		const Links& get_links() const;

		void add_transmission(TransmissionID);
		const Transmissions& get_transmissions() const;

		static unsigned get_port_domain(PortRS*, NodeSystem* context);

	protected:
		unsigned m_id;
		std::string m_name;
		bool m_is_manual;
		bool m_disable_opt;
		Links m_links;
		Transmissions m_xmis;
	};

	class FlowStateOuter
	{
	public:
		using RSDomains = std::vector<DomainRS>;

		RSDomains& get_rs_domains();
		DomainRS* get_rs_domain(unsigned);
		DomainRS& new_rs_domain(unsigned id);

		unsigned new_transmission();
		void add_link_to_transmission(TransmissionID xmis, LinkID link);
		const std::vector<LinkID>& get_transmission_links(TransmissionID xmis);
		unsigned get_n_transmissions() const;
		TransmissionID get_transmission_for_link(LinkID link);
		
		void set_transmissions_exclusive(TransmissionID t1, TransmissionID t2);
		bool are_transmissions_exclusive(TransmissionID t1, TransmissionID t2);

	protected:
		struct TransmissionInfo
		{
			std::vector<LinkID> links;
			std::unordered_set<TransmissionID> exclusive_with;
		};

		RSDomains m_rs_domains;
		std::vector<TransmissionInfo> m_transmissions;
		std::unordered_map<LinkID, TransmissionID> m_link_to_xmis;
	};

	using N2GRemapFunc = std::function<HierObject*(HierObject*)>;
	graph::Graph net_to_graph
	(
		NodeSystem* sys, NetType ntype,
		bool include_internal,
		graph::V2Attr<HierObject*>* v_to_obj,
		graph::Attr2V<HierObject*>* obj_to_v,
		graph::E2Attr<Link*>* e_to_link,
		graph::Attr2E<Link*>* link_to_e,
		const N2GRemapFunc& remap = N2GRemapFunc()
	);

	void do_inner(NodeSystem* sys, unsigned dom_id, FlowStateOuter* fs_out);
	void solve_latency_constraints(NodeSystem* sys, unsigned dom_id);
	void dump_graph(Node* node, NetType net, const std::string& filename, bool labels);
}
}
}