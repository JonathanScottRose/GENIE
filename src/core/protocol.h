#pragma once

#include <limits>
#include <string>
#include <unordered_map>
#include <vector>
#include "prop_macros.h"
#include "bits_val.h"
#include "genie_priv.h"

namespace genie
{
namespace impl
{
	class PortRS;

	struct FieldID
	{
		static const unsigned NO_DOMAIN = std::numeric_limits<unsigned>::max();

		FieldID();
		FieldID(FieldType);
		FieldID(FieldType, unsigned);
		FieldID(FieldType, const std::string&);
		FieldID(FieldType, const std::string&, unsigned);

		bool has_tag() const;
		bool has_domain() const;

		bool matches(const FieldID&) const;
		bool operator<(const FieldID&) const;

		FieldType type;
		std::string tag;
		unsigned domain;
	};

	class FieldInst
	{
	public:
		static const unsigned UNKNOWN_WIDTH = std::numeric_limits<unsigned>::max();

		FieldInst(const FieldID&);
		FieldInst(const FieldID&, unsigned width);

		PROP_GET_SET(id, const FieldID&, m_id);
		PROP_GET_SET(width, unsigned, m_width);

		const std::string& get_tag() const;
		void set_tag(const std::string&);
		bool has_tag() const;
		
		unsigned get_domain() const;
		bool has_domain() const;
		void set_domain(unsigned);
		
		bool matches(const FieldInst&) const;
		bool operator<(const FieldInst&) const;

	protected:
		FieldID m_id;
		unsigned m_width;
	};

	class FieldSet
	{
	public:
		void add(const FieldInst&);
		void add(const FieldSet&);
		void remove(const FieldInst&);
		void subtract(const FieldSet&);
		void intersect(const FieldSet&);
		void clear();
		bool has(const FieldID&) const;
		FieldInst* get(const FieldID&);
		unsigned get_lsb(const FieldID&) const;
		const std::vector<FieldInst>& contents() const;

		unsigned get_width() const;

	protected:
		std::vector<FieldInst> m_fields;
	};

	class CarrierProtocol
	{
	public:
		void clear();
		void add_set(const FieldSet&);

		const FieldSet& jection_fields() const;
		FieldSet domain_fields() const;

		bool has(const FieldID&) const;
		unsigned get_lsb(const FieldID&) const;

		unsigned get_total_width() const;
		unsigned get_domain_lsb() const;
		unsigned get_domain_width() const;

		FieldInst* get_field(const FieldID&);

	protected:
		FieldSet& get_domain_set(unsigned dom) const;

		FieldSet m_jection_set;
		mutable std::vector<std::pair<unsigned, FieldSet>> m_domain_sets;
	};

	class PortProtocol
	{
	public:
		void add_terminal_field(const FieldInst&, const SigRoleID&);
		const FieldSet& terminal_fields() const;
		bool has_terminal_field(const FieldID&) const;
		FieldInst* get_terminal_field(const FieldID&);

		void set_const(const FieldID&, const BitsVal&);
		const BitsVal* get_const(const FieldID&) const;

		const SigRoleID& get_binding(const FieldID&) const;

	protected:
		struct ExtraFieldInfo
		{
			SigRoleID binding;
			bool is_const;
			BitsVal const_val;
		};

		FieldSet m_terminal_fields;
		std::vector<std::pair<FieldID, ExtraFieldInfo>> m_extra_info;
	};

	class ProtocolCarrier
	{
	public:
		CarrierProtocol& get_carried_proto() { return m_carried_proto; }
	private:
		CarrierProtocol m_carried_proto;
	};

	namespace flow
	{
		unsigned calc_transmitted_width(PortRS* src, PortRS* sink);
		void splice_carrier_protocol(PortRS* src, PortRS* sink, ProtocolCarrier*);
	}
}
}