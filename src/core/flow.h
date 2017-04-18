#pragma once

#include <functional>
#include "graph.h"
#include "network.h"

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
		Links& get_links();
		
	protected:
		unsigned m_id;
		std::string m_name;
		Links m_links;
		bool m_is_manual;
	};

	class NodeFlowState
	{
	public:
		using RSDomains = std::vector<DomainRS>;

		NodeFlowState() = default;
		NodeFlowState(const NodeFlowState*, NodeSystem* old_sys, 
			NodeSystem* new_sys);
		~NodeFlowState() = default;

		void reintegrate(NodeFlowState& src);

		const RSDomains& get_rs_domains();
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