#pragma once

#include <unordered_map>
#include "genie/link.h"

namespace genie
{
namespace impl
{
	class LinkRSLogical;

	class AddressRep
	{
	protected:
		mutable unsigned m_size_in_bits;
		mutable bool m_size_needs_recalc;
		std::unordered_map<AddressVal, std::vector<unsigned>> m_addr2xmis;
		std::unordered_map<unsigned, AddressVal> m_xmis2addr;

	public:
		static const AddressVal ADDR_ANY = genie::LinkRS::ADDR_ANY;
		static const AddressVal ADDR_INVALID = ADDR_ANY - 1;

		AddressRep();
		AddressRep(const AddressRep&) = default;

		void insert(unsigned xmis, AddressVal addr);
		std::vector<unsigned> get_xmis(AddressVal addr) const;
		AddressVal get_addr(unsigned xmis) const;
		bool exists(AddressVal addr) const;

		const decltype(m_addr2xmis)& get_addr_bins() const;
		unsigned get_n_addr_bins() const;

		unsigned get_size_in_bits() const;

		bool is_pure_unicast() const;
	};
}
}