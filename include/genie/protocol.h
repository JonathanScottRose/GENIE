#pragma once

#include "genie/common.h"
#include "genie/sigroles.h"
#include "genie/value.h"

namespace genie
{
	typedef unsigned int FieldID;
	const FieldID FIELD_INVALID = std::numeric_limits<FieldID>::max();

	class Field
	{
	public:
		static FieldID reg();

		Field();
		Field(FieldID);
		Field(FieldID, int);
		Field(FieldID, const std::string&, int);

		PROP_GET_SET(id, FieldID, m_id);
		
		PROP_GET(tag, const std::string&, m_tag);
		void set_tag(const std::string&);
		bool has_tag() const;
		
		PROP_GET_SET(width, int, m_width);

		PROP_GET(domain, int, m_domain);
		void set_domain(int);
		bool has_domain() const;
		
		bool matches(const Field&) const;
		bool operator<(const Field&) const;

	protected:
		FieldID m_id;
		bool m_has_tag;
		std::string m_tag;
		int m_width;
		int m_domain;
		bool m_has_domain;
	};

	class FieldSet
	{
	public:
		void add(const Field&);
		void add(const FieldSet&);
		void remove(const Field&);
		void subtract(const FieldSet&);
		void intersect(const FieldSet&);
		void clear();
		bool has(const Field&) const;
		int get_lsb(const Field&) const;
		const List<Field>& contents() const;
		int get_width() const;

	protected:
		List<Field> m_fields;
	};

	class CarrierProtocol
	{
	public:
		void clear();
		void add_set(const FieldSet&);

		const FieldSet& jection_fields() const;
		FieldSet domain_fields() const;

		bool has(const Field&) const;
		int get_lsb(const Field&) const;

		int get_total_width() const;
		int get_domain_lsb() const;
		int get_domain_width() const;

	protected:
		FieldSet m_jection_set;
		std::unordered_map<int, FieldSet> m_domain_sets;
	};

	class PortProtocol
	{
	public:
		PortProtocol();

		static int calc_transmitted_width(const PortProtocol&, const PortProtocol&);

		void add_terminal_field(const Field&, const std::string&);
		const FieldSet& terminal_fields() const;

		void set_const(const Field&, const Value&);
		bool is_const(const Field&) const;
		const Value& get_const_value(const Field&) const;

		bool has(const Field&) const;

		const std::string& get_rvd_tag(const Field&) const;

		PROP_GET_SET(carried_protocol, CarrierProtocol*, m_carry_p);

	protected:
		CarrierProtocol* m_carry_p;
		FieldSet m_terminal_fields;
		List<std::pair<Field, std::string>> m_tag_bindings;
		List<std::pair<Field, Value>> m_const_values;
	};
}