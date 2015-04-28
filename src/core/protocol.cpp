#include "genie/protocol.h"

using namespace genie;

//
// Field
//

FieldID Field::reg()
{
	static FieldID next = 0;
	return next++;
}

Field::Field(FieldID id, const std::string& tag, int width)
	: Field()
{
	set_id(id);
	set_tag(tag);
	set_width(width);
}

Field::Field(FieldID id, int width)
	: Field()
{
	set_id(id);
	set_width(width);
}

Field::Field(FieldID id)
	: Field()
{
	set_id(id);
}

Field::Field()
	: m_has_tag(false),  m_id(FIELD_INVALID), m_has_domain(false), m_domain(-1)
{
}

void Field::set_tag(const std::string& tag)
{
	m_tag = tag;
	m_has_tag = true;
}

void Field::set_domain(int d)
{
	m_domain = d;
	m_has_domain = true;
}

bool Field::has_domain() const
{
	return m_has_domain;
}

bool Field::matches(const Field& o) const
{
	if (m_id != o.m_id)
		return false;

	if (m_has_tag && m_tag != o.m_tag)
		return false;

	if (m_has_domain && m_domain != o.m_domain)
		return false;

	return true;
}

bool Field::operator<(const Field& o) const
{
	if (m_id < o.m_id)
	{
		return true;
	}
	else if (m_id == o.m_id)
	{
		if (m_has_tag && m_tag < o.m_tag)
		{
			return true;
		}
		else if(!m_has_tag || m_tag == o.m_tag)
		{
			if (m_has_domain && m_domain < o.m_domain)
				return true;
		}
	}

	return false;
}

bool Field::has_tag() const
{
	return m_has_tag;
}

//
// FieldSet
//

bool FieldSet::has(const Field& f) const
{
	for (auto& g : m_fields)
	{
		if (g.matches(f))
			return true;
	}

	return false;
}

void FieldSet::clear()
{
	m_fields.clear();
}

void FieldSet::add(const Field& f)
{
	if (!has(f))
		m_fields.push_back(f);

	std::sort(m_fields.begin(), m_fields.end());
}

void FieldSet::add(const FieldSet& s)
{
	for (auto& f : s.contents())
	{
		if (!has(f))
			m_fields.push_back(f);
	}

	std::sort(m_fields.begin(), m_fields.end());
}

void FieldSet::remove(const Field& f)
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
	m_fields.erase(std::remove_if(m_fields.begin(), m_fields.end(), [&](const Field& f)
	{
		return o.has(f);
	}), m_fields.end());

	std::sort(m_fields.begin(), m_fields.end());
}

void FieldSet::intersect(const FieldSet& o)
{
	// intersect: remove things from THIS that are not in O
	m_fields.erase(std::remove_if(m_fields.begin(), m_fields.end(), [&](const Field& f)
	{
		return !o.has(f);
	}), m_fields.end());

	std::sort(m_fields.begin(), m_fields.end());
}

int FieldSet::get_lsb(const Field& f) const
{
	int result = 0;
	for (auto& g : m_fields)
	{
		if (g.matches(f))
			return result;

		result += g.get_width();
	}

	assert(false);
	return -1;
}

int FieldSet::get_width() const
{
	int result = 0;
	for (auto& g : m_fields)
		result += g.get_width();

	return result;
}

const List<Field>& FieldSet::contents() const
{
	return m_fields;
}

//
// PortProtocol
//

PortProtocol::PortProtocol()
	: m_carry_p(nullptr)
{
}

void PortProtocol::add_terminal_field(const Field& f, const std::string& tag)
{
	m_terminal_fields.add(f);
	m_tag_bindings.emplace_back(f, tag);	
}

const FieldSet& PortProtocol::terminal_fields() const
{
	return m_terminal_fields;
}

void PortProtocol::set_const(const Field& f, const Value& v)
{
	auto it = m_const_values.begin();
	for( ; it != m_const_values.end(); ++it)
	{
		if (it->first.matches(f))
			break;
	}

	if (it == m_const_values.end())
		m_const_values.emplace_back(f, v);
	else
		it->second = v;
}

bool PortProtocol::is_const(const Field& f) const
{
	for (auto& i : m_const_values)
	{
		if (i.first.matches(f))
			return true;
	}

	return false;
}

const Value& PortProtocol::get_const_value(const Field& f) const
{
	for (auto& i : m_const_values)
	{
		if (i.first.matches(f))
			return i.second;
	}

	throw Exception("nonexistent field queried for const value");
}

const std::string& PortProtocol::get_rvd_tag(const Field& f) const
{
	for (auto& i : m_tag_bindings)
	{
		if (i.first.matches(f))
			return i.second;
	}

	throw Exception("nonexistent terminal field");
}

int PortProtocol::calc_transmitted_width(const PortProtocol& src, const PortProtocol& sink)
{
	int result = 0;

	// Find the common set of (terminal+jection) fields between src and sink. That's what needs
	// to be transmitted.
	FieldSet src_set = src.terminal_fields();
	CarrierProtocol* src_c = src.get_carried_protocol();
	if (src_c) src_set.add(src_c->jection_fields());

	FieldSet sink_set = sink.terminal_fields();
	CarrierProtocol* sink_c = sink.get_carried_protocol();
	if (sink_c) sink_set.add(sink_c->jection_fields());

	src_set.intersect(sink_set);
	result += src_set.get_width();

	// Add the contribution of domain fields.
	// If both src/sink have a carrier protocol, just take the domain width, because there's no
	// injection/injection of domain fields going on.
	if (src_c && sink_c)
	{
		// arbitrarily take src_c instead of sink_c
		result += src_c->get_domain_width();
	}
	else if (src_c)
	{
		// Otherwise, take the width of the signals crossing from the Terminal region of one
		// side to the Domain region of the other.
		FieldSet common = src_c->domain_fields();
		common.intersect(sink.terminal_fields());
		result += common.get_width();
	}
	else if (sink_c)
	{
		FieldSet common = sink_c->domain_fields();
		common.intersect(src.terminal_fields());
		result += common.get_width();
	}

	return result;
}

bool PortProtocol::has(const Field& f) const
{
	if (m_terminal_fields.has(f))
		return true;

	if (m_carry_p && m_carry_p->has(f))
		return true;

	return false;
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
			m_domain_sets[f.get_domain()].add(f);
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

bool CarrierProtocol::has(const Field& f) const
{
	bool result = m_jection_set.has(f);
	if (!result && f.has_domain())
	{
		result = m_domain_sets.at(f.get_domain()).has(f);
	}
	return result;
}

int CarrierProtocol::get_lsb(const Field& f) const
{
	if (m_jection_set.has(f))
		return m_jection_set.get_lsb(f);

	if (f.has_domain())
	{
		const auto& s = m_domain_sets.at(f.get_domain());
		if (s.has(f))
			return get_domain_lsb() + s.get_lsb(f);
	}

	throw Exception("nonexistent carried field");
}

int CarrierProtocol::get_total_width() const
{
	int jection_width = m_jection_set.get_width();
	return jection_width + get_domain_width();
}

int CarrierProtocol::get_domain_lsb() const
{
	return m_jection_set.get_width();
}

int CarrierProtocol::get_domain_width() const
{
	int domain_width = 0;
	for (const auto& i : m_domain_sets)
	{
		domain_width = std::max(domain_width, i.second.get_width());
	}
	
	return domain_width;
}

