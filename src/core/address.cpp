#include "pch.h"
#include "address.h"


using namespace genie::impl;
using genie::AddressVal;

AddressRep::AddressRep()
	: m_size_in_bits(0), m_size_needs_recalc(true)
{
}

void AddressRep::insert(unsigned xmis, AddressVal addr)
{
	m_addr2xmis[addr].push_back(xmis);
	m_xmis2addr[xmis] = addr;
	m_size_needs_recalc = true;
}

std::vector<unsigned> AddressRep::get_xmis(AddressVal addr) const
{
	std::vector<unsigned> result;
	auto it = m_addr2xmis.find(addr);
	if (it != m_addr2xmis.end())
		std::copy(it->second.begin(), it->second.end(), result.begin());
	
	return result;
}

AddressVal AddressRep::get_addr(unsigned xmis) const
{
	auto it = m_xmis2addr.find(xmis);
	return it == m_xmis2addr.end() ? ADDR_INVALID : it->second;
}

bool AddressRep::exists(AddressVal addr) const
{
	return m_addr2xmis.count(addr) != 0;
}

auto AddressRep::get_addr_bins() const -> const decltype(m_addr2xmis)&
{
	return m_addr2xmis;
}

unsigned AddressRep::get_n_addr_bins() const
{
	return m_addr2xmis.size();
}

unsigned AddressRep::get_size_in_bits() const
{
	if (m_size_needs_recalc)
	{
		m_size_in_bits = 0;

		// Get max value of all addresses
		for (auto it : m_addr2xmis)
		{
			AddressVal addr = it.first;

			// ADDR_ANY represents "no representation"
			if (addr == ADDR_ANY)
				continue;
			else if (addr == ADDR_INVALID)
				assert(false); // shouldn't be any of these in here

			unsigned bits = util::log2(addr + 1);
			m_size_in_bits = std::max(m_size_in_bits, bits);
		}

		m_size_needs_recalc = false;
	}

	return m_size_in_bits;
}

bool AddressRep::is_pure_unicast() const
{
	// Examine all address values to make sure they contain exactly one '1' each
	for (auto& entry : m_xmis2addr)
	{
		auto addr = entry.second;

		if (util::popcnt(addr) > 1)
			return false;
	}

	return true;
}


