#pragma once

#include <unordered_set>
#include "ct/common.h"

namespace ct
{
namespace P2P
{
	class Field;
	class FieldState;
	class PhysField;
	class FieldSet;

	class Field
	{
	public:
		Field();
		Field(const std::string&, int);
		PROP_GET(name, const std::string&, name);

		std::string name;
		int width;
	};

	class FieldSet
	{
	public:
		typedef std::vector<Field> Fields;

		const Fields& fields() const { return m_fields; }
		void add_field(const Field&);
		void remove_field(const std::string& name);
		bool has_field(const std::string& name) const;
		Field& get_field() const;
		void clear();
		
	protected:
		Fields m_fields;
		static std::function<bool(const Field&)> finder(const std::string& name);
	};

	class FieldState
	{
	public:
		FieldState();
		PROP_GET(name, const std::string&, name);

		// which field's state?
		std::string name;

		// constant value
		bool is_const;
		int const_value;

		// locally produced/consumed (as opposed to carried)?
		bool is_local;

		// physical encapsulation
		std::string phys_field;
		int phys_field_lo;
	};

	class PhysField
	{
	public:
		enum Sense
		{
			FWD,
			REV
		};

		typedef std::unordered_set<std::string> Fields;

		PhysField();
		PROP_GET(name, const std::string&, name);

		void add_field(const std::string& name);
		bool has_field(const std::string& name) const;
		void delete_field(const std::string& name);
		Fields& get_fields() { return this->fields; }

		std::string name;
		int width;
		Fields fields; // contained logical fields
		Sense sense;
		
		std::vector<Fields> sets; // for compaction
	};

	class Protocol
	{
	public:
		Protocol();
		Protocol(const Protocol&);
		~Protocol();

		Protocol& operator=(const Protocol&);

		PROP_DICT(Fields, field, Field);
		PROP_DICT(FieldStates, field_state, FieldState);
		PROP_DICT(PhysFields, phys_field, PhysField);

		Field* init_field(const std::string& name, int width, PhysField::Sense sense = PhysField::FWD);
		PhysField* init_physfield(const std::string& name, int width, PhysField::Sense sense =
			PhysField::FWD);

		void carry_fields(const FieldSet& set, const std::string& targ_phys);
		void allocate_bits();
		void copy_carriage(const Protocol& src, const std::string& pf);
	};
}
}