#include "ct/p2p.h"

using namespace ct;
using namespace ct::P2P;

//
// Protocol
//

Protocol::Protocol()
{
}

Protocol::Protocol(const Protocol& other)
{
	*this = other;
}

Protocol& Protocol::operator= (const Protocol& other)
{
	Util::delete_all_2(m_fields);
	Util::delete_all_2(m_phys_fields);
	Util::delete_all_2(m_field_states);

	Util::copy_all_2(other.m_fields, m_fields);
	Util::copy_all_2(other.m_phys_fields, m_phys_fields);
	Util::copy_all_2(other.m_field_states, m_field_states);

	return *this;
}

Protocol::~Protocol()
{
	Util::delete_all_2(m_fields);
	Util::delete_all_2(m_phys_fields);
	Util::delete_all_2(m_field_states);
}

Field* Protocol::init_field(const std::string& name, int width, PhysField::Sense sense)
{
	// convenience function: handles the common case of adding a physical field containing one
	// logical field, all with the same name and width. enough for most cases with no encapsulation.

	auto f = new Field(name, width);
	add_field(f);

	auto fs = new FieldState;
	fs->name = name;
	fs->phys_field = name;
	fs->phys_field_lo = 0;

	add_field_state(fs);

	auto pf = init_physfield(name, width, sense);
	pf->add_field(name);
	pf->sets.push_back(PhysField::Fields({name}));

	return f;
}

PhysField* Protocol::init_physfield(const std::string& name, int width, PhysField::Sense sense)
{
	auto pf = new PhysField;
	pf->name = name;
	pf->width = width;
	pf->sense = sense;

	add_phys_field(pf);

	return pf;
}

void Protocol::carry_fields(const FieldSet& set, const std::string& targ_phys)
{
	PhysField::Fields tmpset;
	auto pf = get_phys_field(targ_phys);

	// update protocol structures to add each field in the set
	for (auto& f : set.fields())
	{
		// add to fields
		if (!has_field(f.name))
		{
			add_field(new Field(f));
		}
		else
		{
			m_fields[f.name]->width = std::max(m_fields[f.name]->width, f.width);
		}

		// initialize field state
		FieldState* fs = get_field_state(f.name);
		if (!fs)
		{
			fs = new FieldState;
			fs->name = f.name;
			fs->phys_field_lo = 0;
			add_field_state(fs);
		}

		fs->phys_field = targ_phys;
		fs->is_local = false;

		// modify target physfield
		if (!pf->has_field(f.name))
			pf->add_field(f.name);

		tmpset.insert(f.name);
	}

	// add this set of fields for later compaction algorithm
	if (std::find(pf->sets.begin(), pf->sets.end(), tmpset) == pf->sets.end())
	{
		pf->sets.push_back(tmpset);
	}
}

void Protocol::allocate_bits()
{
	for (auto& i : phys_fields())
	{
		PhysField* pf = i.second;

		// stupid bit allocation - make all carried fields non-overlapping.
		// this is sub-optimal but guarantees they can be accessed/swizzled anywhere

		int cur_lo = 0;

		for (auto& fname : pf->fields)
		{
			// initialize each field's position to 'unallocated'
			m_field_states[fname]->phys_field_lo = cur_lo;
			cur_lo += m_fields[fname]->width;
		}

		pf->width = cur_lo;

		// clean up unused data structures in the physfield
		pf->sets.clear();
	}
}

void Protocol::copy_carriage(const Protocol& src, const std::string& pfname)
{
	// for all fields carried by src_protocol's src_physfield, do this for the ones that we don't have:
	// 1) create a logical field entry
	// 2) create a fieldstate with appropriate carriage info
	// 3) add it to our physfield's set of carried fields

	for (auto& fname : src.get_phys_field(pfname)->fields)
	{
		if (this->has_field(fname))
			continue;

		// 1
		Field* f = new Field(*src.get_field(fname));
		add_field(f);

		// 2
		FieldState* fs = new FieldState(*src.get_field_state(fname));
		add_field_state(fs);

		// 3
		get_phys_field(pfname)->add_field(fname);
	}

	// 4: copy other physfield stuff
	get_phys_field(pfname)->width = src.get_phys_field(pfname)->width;
}

//
// Field
//

Field::Field()
{
}

Field::Field(const std::string& name_, int width_)
: name(name_), width(width_)
{
}

//
// FieldState
//

FieldState::FieldState()
: is_const(false), is_local(true)
{
}

//
// PhysField
//

PhysField::PhysField()
{
}

void PhysField::add_field(const std::string& name)
{
	fields.insert(name);
}

bool PhysField::has_field(const std::string& name) const
{
	return fields.count(name) > 0;
}

void PhysField::delete_field(const std::string& name)
{
	fields.erase(name);
}

//
// FieldSet
//

std::function<bool(const Field&)> FieldSet::finder(const std::string& name)
{
	return [=](const Field& o)
	{
		return o.name == name;
	};
}

void FieldSet::add_field(const Field& f)
{
	auto exist = std::find_if(m_fields.begin(), m_fields.end(), finder(f.name));

	if (exist != m_fields.end())
	{
		exist->width = std::max(exist->width, f.width);
	}
	else
	{
		m_fields.push_back(f);
	}
}

void FieldSet::remove_field(const std::string& name)
{
	auto exist = std::find_if(m_fields.begin(), m_fields.end(), finder(name));
	if (exist != m_fields.end())
		m_fields.erase(exist);
}

bool FieldSet::has_field(const std::string& name) const
{
	auto exist = std::find_if(m_fields.begin(), m_fields.end(), finder(name));
	return (exist != m_fields.end());
}

void FieldSet::clear()
{
	m_fields.clear();
}