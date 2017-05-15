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
	class DomainRS
	{
	public:
		static constexpr unsigned INVALID = std::numeric_limits<unsigned>::max();
		using Links = std::vector<LinkID>;
		using Transmissions = std::vector<unsigned>;

		DomainRS();
		DomainRS(const DomainRS&) = default;
		~DomainRS() = default;

		PROP_GET_SET(id, unsigned, m_id);
		PROP_GET_SET(is_manual, bool, m_is_manual);
		PROP_GET_SET(name, const std::string&, m_name);

		void add_link(LinkID);
		const Links& get_links() const;

		void add_transmission(unsigned);
		const Transmissions& get_transmissions() const;

		static unsigned get_port_domain(PortRS*, NodeSystem* context);

	protected:
		unsigned m_id;
		std::string m_name;
		bool m_is_manual;
		Links m_links;
		Transmissions m_xmis;
	};

	class FlowStateOuter
	{
	public:
		using RSDomains = std::vector<DomainRS>;

		NodeSystem* sys = nullptr;

		const RSDomains& get_rs_domains() const;
		DomainRS* get_rs_domain(unsigned);
		DomainRS& new_rs_domain(unsigned id);

		unsigned new_transmission();
		void add_link_to_transmission(unsigned xmis, LinkID link);
		const std::vector<LinkID>& get_transmission_links(unsigned xmis);
		unsigned get_n_transmissions() const;
		unsigned get_transmission_for_link(LinkID link);

	protected:
		struct TransmissionInfo
		{
			std::vector<LinkID> links;
		};

		RSDomains m_rs_domains;
		std::vector<TransmissionInfo> m_transmissions;
		std::unordered_map<LinkID, unsigned> m_link_to_xmis;
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
}
}
}