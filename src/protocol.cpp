#include "p2p.h"

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
	pf->add_field(name);
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

		// for each field, how many sets does it show up in?
		std::unordered_map<std::string, int> occurrence;

		// an ordering for visiting the fields during bit-assignment
		std::vector<std::string> field_order;
		
		// the co-field set for each field. this is the set of all fields that a given field
		// will ever be simultaneously transmitted with, in the same clock cycle.
		std::unordered_map<Field*, std::unordered_set<std::string>> co_fields;

		for (auto& fname : pf->fields)
		{
			// add here, sort later
			field_order.push_back(fname);

			// initialize each field's position to 'unallocated'
			m_field_states[fname]->phys_field_lo = -1;

			Field* f = m_fields[fname];

			// for each carriage set...
			for (auto& set : pf->sets)
			{
				// does this field exist in this set? 0 or 1
				int count = set.count(fname);

				// count how many sets each field appears in (used for sorting)
				occurrence[fname] += count;

				// if a field shows up in a set, every other field in that set is added to that 
				// field's co-field set
				if (count)
				{
					// insert the whole set (which includes f...) and remove f from each set later
					co_fields[f].insert(set.begin(), set.end());
				}
			}

			// remove each field from its own co-field set
			co_fields[f].erase(fname);
		}

		// create field ordering by descending number of co-temporal sets a field appears in.
		// this creates a greedy ordering by trying to allocate the most-common fields first, minimizing
		// wasteage of bit-space
		std::sort(field_order.begin(), field_order.end(), [&](const std::string& a, const std::string& b)
		{
			return occurrence[a] > occurrence[b];
		});

		// now go through each field and assign/fix its bit-location inside the parent field.
		// for each field, we have a proposed position POS, which is initially 0.
		// bits [POS+field_size-1 : POS] must not already be occupied by any other field that is ever
		// co-temporally transmitted with it.
		// POS gradually advances until one can be found that's agreed in all transmission configurations.
		//

		for (auto& fname : field_order)
		{
			int pos = 0; // proposed position of field
			bool found_pos = false; // have we found an agreed-upon position yet?

			Field* f = m_fields[fname];
			FieldState* fs = m_field_states[fname];

			while (!found_pos)
			{
				found_pos = true;

				// test this proposed location against all already-allocated fields in the co-field set
				for (auto& co_name : co_fields[f])
				{
					FieldState* co_fs = m_field_states[co_name];
					Field* co_f = m_fields[co_name];

					// do not test against un-allocated fields
					if (co_fs->phys_field_lo < 0)
						continue;
					
					// check for overlap
					int lo1 = pos;
					int hi1 = pos + f->width;
					int lo2 = co_fs->phys_field_lo;
					int hi2 = lo2 + co_f->width;

					if (lo1 < hi2 && hi1 > lo2)
					{
						// overlap. try again with a new bit position (advance to the obstructing
						// co-field's end)
						found_pos = false;
						pos = hi2;
						break;
					}
				}
			}

			// Allocate the field, give it a position
			fs->phys_field_lo = pos;

			// Expand the size of the physfield to accommodate
			pf->width = std::max(pf->width, fs->phys_field_lo + f->width);
		}

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