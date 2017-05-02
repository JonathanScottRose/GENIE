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
	class NodeSystem;
	class LinkRSLogical;

namespace flow
{
	class DomainRS
	{
	public:
		static constexpr unsigned INVALID = std::numeric_limits<unsigned>::max();
		using Links = std::vector<LinkRSLogical*>;

		DomainRS();
		DomainRS(const DomainRS&) = default;
		~DomainRS() = default;

		PROP_GET_SET(id, unsigned, m_id);
		PROP_GET_SET(is_manual, bool, m_is_manual);
		PROP_GET_SET(name, const std::string&, m_name);

		void add_link(LinkRSLogical*);
		const Links& get_links() const;

	protected:
		unsigned m_id;
		std::string m_name;
		bool m_is_manual;
		Links m_links;
	};

	class FlowStateInner
	{
	public:
		AddressRep& get_flow_rep() { return m_flow_rep; }

		unsigned new_transmission();
		void add_link_to_transmission(unsigned xmis, LinkRSLogical* link);
		const std::vector<LinkRSLogical*>& get_transmission_links(unsigned id);
		PortRS* get_transmission_src(unsigned id);
		unsigned get_n_transmissions() const;
		unsigned get_transmission_for_link(LinkRSLogical*);

	protected:
		struct TransmissionInfo
		{
			std::vector<LinkRSLogical*> links;
			PortRS* src;
		};

		AddressRep m_flow_rep;
		std::vector<TransmissionInfo> m_transmissions;
		std::unordered_map<LinkRSLogical*, unsigned> m_link_to_xmis;
	};

	class FlowStateOuter
	{
	public:
		using RSDomains = std::vector<DomainRS>;

		FlowStateOuter() = default;
		FlowStateOuter(const FlowStateOuter*, const NodeSystem* old_sys, 
			NodeSystem* new_sys);
		~FlowStateOuter() = default;

		void reintegrate(FlowStateOuter& src);

		const RSDomains& get_rs_domains() const;
		DomainRS* get_rs_domain(unsigned);
		DomainRS& new_rs_domain(unsigned id);

	protected:
		RSDomains m_rs_domains;
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

	void do_inner(NodeSystem* sys);
}
}
}