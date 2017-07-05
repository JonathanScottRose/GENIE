#include "pch.h"
#include "protocol.h"
#include "port_rs.h"
#include "net_rs.h"
#include "node.h"

using namespace genie::impl;

//
// FieldID
//

FieldID::FieldID()
	: FieldID(FIELD_INVALID)
{
}

FieldID::FieldID(FieldType type_)
	: FieldID(type_, NO_DOMAIN)
{
}

FieldID::FieldID(FieldType type_, unsigned domain_)
	: FieldID(type_, "", domain_)
{
}

FieldID::FieldID(FieldType type_, const std::string & tag_)
	: FieldID(type_, tag_, NO_DOMAIN)
{
}

FieldID::FieldID(FieldType type_, const std::string & tag_, unsigned domain_)
	: type(type_), tag(tag_), domain(domain_)
{
}

bool FieldID::has_tag() const
{
	return !tag.empty();
}

bool FieldID::has_domain() const
{
	return domain != NO_DOMAIN;
}

bool FieldID::matches(const FieldID& o) const
{
	return (type == o.type && tag == o.tag && domain == o.domain);
}

bool FieldID::operator<(const FieldID& o) const
{
	if (type < o.type)
	{
		return true;
	}
	else if (type == o.type)
	{
		if (tag < o.tag)
		{
			return true;
		}
		else if (tag == o.tag)
		{
			return domain < o.domain;
		}
	}

	return false;
}

//
// FieldInst
//

FieldInst::FieldInst(const FieldID &id)
	: FieldInst(id, UNKNOWN_WIDTH)
{
}

FieldInst::FieldInst(const FieldID& id, unsigned width)
	: m_id(id), m_width(width)
{
}

const std::string & FieldInst::get_tag() const
{
	return m_id.tag;
}

void FieldInst::set_tag(const std::string& tag)
{
	m_id.tag = tag;
}

bool FieldInst::has_tag() const
{
	return m_id.has_tag();
}

unsigned FieldInst::get_domain() const
{
	return m_id.domain;
}

bool FieldInst::has_domain() const
{
	return m_id.has_domain();
}

void FieldInst::set_domain(unsigned dom)
{
	m_id.domain = dom;
}

bool FieldInst::matches(const FieldInst& o) const
{
	return m_id.matches(o.m_id);
}

bool FieldInst::operator<(const FieldInst &o) const
{
	return m_id < o.m_id;
}

//
// FieldSet
//

bool FieldSet::has(const FieldID& f) const
{
	for (auto& g : m_fields)
	{
		if (g.get_id().matches(f))
			return true;
	}

	return false;
}

FieldInst * FieldSet::get(const FieldID &fid)
{
	for (auto& i : m_fields)
	{
		if (i.matches(fid))
			return &i;
	}

	return nullptr;
}

void FieldSet::clear()
{
	m_fields.clear();
}

void FieldSet::add_nocheck(const FieldInst &f)
{
	m_fields.push_back(f);
}

void FieldSet::add(const FieldInst& f)
{
	// Add in sorted order
	auto ins_loc = m_fields.end();
	bool found_ins = false;

	for (auto it = m_fields.begin(); it != m_fields.end(); ++it)
	{
		auto& g = *it;

		// Find the first possible insert location in the already-sorted list
		if (!found_ins && f < g)
		{
			found_ins = true;
			ins_loc = it;
		}
		else if (f.matches(g))
		{
			// Do not insert duplicates
			return;
		}
	}

	m_fields.insert(ins_loc, f);
}

void FieldSet::add(const FieldSet& s)
{
	for (auto& f : s.contents())
	{
		if (!has(f.get_id()))
			m_fields.push_back(f);
	}

	std::sort(m_fields.begin(), m_fields.end());
}

void FieldSet::remove(const FieldInst& f)
{
	for (auto it = m_fields.begin(); it != m_fields.end(); ++it)
	{
		if (it->matches(f))
		{
			m_fields.erase(it);
			break;
		}
	}
}

void FieldSet::subtract(const FieldSet& o)
{
	// subtract: remove things from THIS that are in O
	m_fields.erase(std::remove_if(m_fields.begin(), m_fields.end(), [&](const FieldInst& f)
	{
		return o.has(f.get_id());
	}), m_fields.end());
}

void FieldSet::intersect(const FieldSet& o)
{
	// intersect: remove things from THIS that are not in O
	m_fields.erase(std::remove_if(m_fields.begin(), m_fields.end(), [&](const FieldInst& f)
	{
		return !o.has(f.get_id());
	}), m_fields.end());

	std::sort(m_fields.begin(), m_fields.end());
}

unsigned FieldSet::get_lsb(const FieldID& f) const
{
	int result = 0;
	for (auto& g : m_fields)
	{
		if (g.get_id().matches(f))
			return result;

		unsigned width = g.get_width();
		assert(width != FieldInst::UNKNOWN_WIDTH);
		result += width;
	}

	assert(false);
	return 0;
}

unsigned FieldSet::get_width() const
{
	int result = 0;
	for (auto& g : m_fields)
	{
		unsigned width = g.get_width();
		assert(width != FieldInst::UNKNOWN_WIDTH);
		result += width;
	}

	return result;
}

const std::vector<FieldInst>& FieldSet::contents() const
{
	return m_fields;
}

//
// PortProtocol
//

void PortProtocol::add_terminal_field(const FieldInst& f, const SigRoleID& bnd)
{
	m_terminal_fields.add(f);
	ExtraFieldInfo inf = { bnd, false, BitsVal() };
	m_extra_info.emplace_back(f.get_id(), inf);
}

const FieldSet& PortProtocol::terminal_fields() const
{
	return m_terminal_fields;
}

FieldSet PortProtocol::terminal_fields_nonconst() const
{
	FieldSet result;
	for (auto i : m_terminal_fields.contents())
	{
		if (this->get_const(i.get_id()) == nullptr)
			result.add_nocheck(i);
	}

	return result;
}

void PortProtocol::set_const(const FieldID& f, const BitsVal& v)
{
	auto it = m_extra_info.begin();
	for( ; it != m_extra_info.end(); ++it)
	{
		if (it->first.matches(f))
			break;
	}

	if (it == m_extra_info.end())
	{
		ExtraFieldInfo inf = { SigRoleType::INVALID, true, v };
		m_extra_info.emplace_back(f, inf);
	}
	else
	{
		it->second.const_val = v;
		it->second.is_const = true;
	}
}

const BitsVal* PortProtocol::get_const(const FieldID& f) const
{
	for (auto& i : m_extra_info)
	{
		if (i.first.matches(f) && i.second.is_const)
		{
			return &i.second.const_val;
		}	
	}

	return nullptr;
}

const SigRoleID& PortProtocol::get_binding(const FieldID& f) const
{
	for (auto& i : m_extra_info)
	{
		if (i.first.matches(f))
		{
			return i.second.binding;
		}
	}

	throw Exception("nonexistent terminal field");
}

bool PortProtocol::has_terminal_field(const FieldID& f) const
{
	if (m_terminal_fields.has(f))
		return true;
	
	return false;
}

FieldInst * PortProtocol::get_terminal_field(const FieldID & fid)
{
	return m_terminal_fields.get(fid);
}

//
// CarrierProtocol
//

void CarrierProtocol::clear()
{
	m_jection_set.clear();
	m_domain_sets.clear();
}

void CarrierProtocol::add_set(const FieldSet& s)
{
	for (const auto& f : s.contents())
	{
		if (f.has_domain())
		{
			get_domain_set(f.get_domain()).add(f);
		}
		else
		{
			m_jection_set.add(f);
		}
	}
}

const FieldSet& CarrierProtocol::jection_fields() const
{
	return m_jection_set;
}

FieldSet CarrierProtocol::domain_fields() const
{
	FieldSet result;
	for (auto& i : m_domain_sets)
		result.add(i.second);

	return result;
}

bool CarrierProtocol::has(const FieldID& f) const
{
	bool result = m_jection_set.has(f);
	if (!result && f.has_domain())
	{
		result = get_domain_set(f.domain).has(f);
	}
	return result;
}

unsigned CarrierProtocol::get_lsb(const FieldID& f) const
{
	if (m_jection_set.has(f))
		return m_jection_set.get_lsb(f);

	if (f.has_domain())
	{
		const auto& s = get_domain_set(f.domain);
		if (s.has(f))
			return get_domain_lsb() + s.get_lsb(f);
	}

	throw Exception("nonexistent carried field");
}

unsigned CarrierProtocol::get_total_width() const
{
	unsigned jection_width = m_jection_set.get_width();
	return jection_width + get_domain_width();
}

unsigned CarrierProtocol::get_domain_lsb() const
{
	return m_jection_set.get_width();
}

unsigned CarrierProtocol::get_domain_width() const
{
	unsigned domain_width = 0;
	for (const auto& i : m_domain_sets)
	{
		domain_width = std::max(domain_width, i.second.get_width());
	}
	
	return domain_width;
}

FieldInst * CarrierProtocol::get_field(const FieldID &fid)
{
	FieldInst* result = nullptr;

	result = m_jection_set.get(fid);
	if (!result)
	{
		for (auto& set : m_domain_sets)
		{
			if (result = set.second.get(fid))
				break;
		}
	}

	return result;
}

FieldSet& CarrierProtocol::get_domain_set(unsigned dom) const
{
	for (auto& i : m_domain_sets)
	{
		if (i.first == dom)
		{
			return i.second;
		}
	}

	// not found, make new empty set for domain
	m_domain_sets.emplace_back(dom, FieldSet());
	return m_domain_sets.back().second;
}


unsigned flow::calc_transmitted_width(PortRS* src, PortRS* sink)
{
	unsigned result = 0;

	// Get src and sink port protocols
	PortProtocol& src_proto = src->get_proto();
	PortProtocol& sink_proto = sink->get_proto();

	// Find the common set of (terminal+jection) fields between src and sink. That's part
	// of what's transmitted between the two.
	FieldSet src_set = src_proto.terminal_fields();
	CarrierProtocol* src_carr = src->get_carried_proto();
	if (src_carr) src_set.add(src_carr->jection_fields());

	FieldSet sink_set = sink_proto.terminal_fields();
	CarrierProtocol* sink_carr = sink->get_carried_proto();
	if (sink_carr) sink_set.add(sink_carr->jection_fields());

	src_set.intersect(sink_set);
	result += src_set.get_width();

	// Add the contribution of domain fields.
	// If both src/sink have a carrier protocol, just take the domain width, because there's no
	// injection/ejection of domain fields going on.
	if (src_carr && sink_carr)
	{
		// arbitrarily take src_c instead of sink_c
		result += src_carr->get_domain_width();
	}
	else if (src_carr)
	{
		// Otherwise, take the width of the signals crossing from the Terminal region of one
		// side to the Domain region of the other.
		FieldSet common = src_carr->domain_fields();
		common.intersect(sink_proto.terminal_fields());
		result += common.get_width();
	}
	else if (sink_carr)
	{
		FieldSet common = sink_carr->domain_fields();
		common.intersect(src_proto.terminal_fields());
		result += common.get_width();
	}

	return result;
}

unsigned flow::calc_transmitted_width(LinkRSPhys* link)
{
	auto src = static_cast<PortRS*>(link->get_src());
	auto sink = static_cast<PortRS*>(link->get_sink());
	return calc_transmitted_width(src, sink);
}

void flow::splice_carrier_protocol(PortRS * src, PortRS * sink, ProtocolCarrier * node)
{
	// src and sink already have well-populated protocol info.
	// 'node' is a newly-spliced protocol carrier that does NOT introduce any new terminal fields.
	// this function populates node's carrierprotocol in a lightweight fashion.

	auto& src_proto = src->get_proto();
	auto& sink_proto = sink->get_proto();
	auto& car_proto = node->get_carried_proto();

	FieldSet carriage_set = sink_proto.terminal_fields();

	auto sink_node = dynamic_cast<ProtocolCarrier*>(sink->get_node());
	if (sink_node)
	{
		auto& dstream_car_proto = sink_node->get_carried_proto();
		carriage_set.add(dstream_car_proto.domain_fields());
		carriage_set.add(dstream_car_proto.jection_fields());
	}

	FieldSet src_set = src_proto.terminal_fields();

	auto src_node = dynamic_cast<ProtocolCarrier*>(src->get_node());
	if (src_node)
	{
		auto& ustream_car_proto = src_node->get_carried_proto();
		src_set.add(ustream_car_proto.domain_fields());
		src_set.add(ustream_car_proto.jection_fields());
	}

	// Only carry the things that are common to src and sink.
	carriage_set.intersect(src_set);
	car_proto.add_set(carriage_set);
}
