#pragma once

#include <unordered_map>
#include "genie/link.h"

namespace genie
{
namespace impl
{
	class LinkRSLogical;
	class NodeSystem;
	class NodeSplit;
	class PortRS;

	class AddressRep
	{
	public:
		static const unsigned ADDR_ANY = genie::LinkRS::ADDR_ANY;
		static const unsigned ADDR_INVALID = ADDR_ANY - 1;

		AddressRep();
		AddressRep(const AddressRep&) = default;

		void insert(unsigned xmis, unsigned addr);
		std::vector<unsigned> get_xmis(unsigned addr) const;
		unsigned get_addr(unsigned xmis) const;
		bool exists(unsigned addr) const;

		std::vector<unsigned> get_addr_bins() const;
		unsigned get_n_addr_bins() const;

		unsigned get_size_in_bits();

	protected:
		unsigned m_size_in_bits;
		bool m_size_needs_recalc;
		std::unordered_map<unsigned, std::vector<unsigned>> m_addr2xmis;
		std::unordered_map<unsigned, unsigned> m_xmis2addr;
	};

	namespace flow
	{
		void make_internal_flow_rep(NodeSystem* sys);
		AddressRep make_split_node_rep(NodeSystem*, NodeSplit*);
		AddressRep make_srcsink_flow_rep(NodeSystem*, PortRS*);
	}
}
}