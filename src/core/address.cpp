#include "pch.h"
#include "address.h"
#include "node_system.h"
#include "node_split.h"
#include "port_rs.h"
#include "flow.h"
#include "net_rs.h"

using namespace genie::impl;

AddressRep::AddressRep()
	: m_size_in_bits(0), m_size_needs_recalc(true)
{
}

void AddressRep::insert(unsigned xmis, unsigned addr)
{
	m_addr2xmis[addr].push_back(xmis);
	m_xmis2addr[xmis] = addr;
	m_size_needs_recalc = true;
}

std::vector<unsigned> AddressRep::get_xmis(unsigned addr) const
{
	std::vector<unsigned> result;
	auto it = m_addr2xmis.find(addr);
	if (it != m_addr2xmis.end())
		std::copy(it->second.begin(), it->second.end(), result.begin());
	
	return result;
}

unsigned AddressRep::get_addr(unsigned xmis) const
{
	auto it = m_xmis2addr.find(xmis);
	return it == m_xmis2addr.end() ? ADDR_INVALID : it->second;
}

bool AddressRep::exists(unsigned addr) const
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
			unsigned addr = it.first;

			// ADDR_ANY represents "no representation"
			if (addr == ADDR_ANY)
				continue;
			else if (addr == ADDR_INVALID)
				assert(false); // shouldn't be any of these in here

			unsigned bits = util::log2((int)addr + 1);
			m_size_in_bits = std::max(m_size_in_bits, bits);
		}

		m_size_needs_recalc = false;
	}

	return m_size_in_bits;
}


void flow::make_internal_flow_rep(NodeSystem* sys, unsigned dom_id)
{
	auto fs_in = sys->get_flow_state_inner();
	auto fs_out = sys->get_flow_state_outer();
	auto& rep = fs_in->get_flow_rep();
	auto dom = fs_out->get_rs_domain(dom_id);
	auto& dom_xmis = dom->get_transmissions();

	// Take each transmission in one domain, and give it a unique ID
	unsigned n_xmis = dom_xmis.size();
	for (unsigned i = 0; i < n_xmis; i++)
	{
		unsigned xmis_id = dom_xmis[i];
		rep.insert(xmis_id, i);
	}
}

AddressRep flow::make_split_node_rep(NodeSystem* sys, NodeSplit* sp)
{
	AddressRep result;

	auto fs_out = sys->get_flow_state_outer();
	auto link_rel = sys->get_link_relations();

	// transmission ID to address.
	std::unordered_map<unsigned, unsigned> trans2addr;

	// Find out which outputs each transmission's flows go to, and create
	// a one-hot mask out of that.
	unsigned n_out = sp->get_n_outputs();
	for (unsigned i = 0; i < n_out; i++)
	{
		auto port = sp->get_output(i);
		auto out_link = port->get_endpoint(NET_RS_PHYS, Port::Dir::OUT)->get_link0();
		
		// Get logical links from physical link
		auto rs_links = link_rel->get_parents(out_link->get_id(), NET_RS_LOGICAL);

		for (auto rs_link : rs_links)
		{
			// Find the transmission and its ID for each rs link.
			// Add to bitmask
			auto xmis_id = fs_out->get_transmission_for_link(rs_link);

			trans2addr[xmis_id] |= (1 << i);
		}
	}

	// Copy address assignments to result
	for (auto it : trans2addr)
	{
		result.insert(it.first, it.second);
	}

	return result;
}

AddressRep flow::make_srcsink_flow_rep(NodeSystem* sys, PortRS* srcsink)
{
	AddressRep result;

	auto fs_out = sys->get_flow_state_outer();

	// Find out whether we're dealing with a source or sink.
	auto dir = srcsink->get_effective_dir(sys);

	// Get the appropriate endpoint
	auto ep = srcsink->get_endpoint(NET_RS_LOGICAL, dir);

	// Get links
	auto& links = ep->links();

	// Bin by src or sink address
	for (auto link : links)
	{
		auto rs_link = static_cast<LinkRSLogical*>(link);
		auto xmis_id = fs_out->get_transmission_for_link(rs_link->get_id());

		auto user_addr = dir == Port::Dir::OUT ?
			rs_link->get_src_addr() : rs_link->get_sink_addr();

		result.insert(xmis_id, user_addr);
	}

	return result;
}