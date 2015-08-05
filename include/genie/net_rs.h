#pragma once

#include "genie/networks.h"
#include "genie/structure.h"
#include "genie/value.h"
#include "genie/protocol.h"

namespace genie
{
	class RSPort;
	class RSLinkpoint;
	class RSLink;
	class ClockPort;
	class TopoPort;
	class RVDPort;

	extern const NetType NET_RS;

	class RSLink : public Link
	{
	public:
		RSLink();

		static List<RSLink*> get_from_rvd_port(RVDPort*);
		
		bool contained_in_rvd_port(RVDPort*) const;

		PROP_GET_SETR(flow_id, Value&, m_flow_id);
		PROP_GET_SET(domain_id, int, m_domain_id);

		Link* clone() const override;

	protected:
		int m_domain_id;
		Value m_flow_id;
	};

	// Attaches to an RSLink to indicate mutually-exclusive communications
	class ARSExclusionGroup : public AspectWithRef<RSLink>
	{
	public:
		// Get the set (excluding this/owning link)
		const List<RSLink*>& get_others() const;

		// Union-adds new members (this/owning link filtered out)
		void add(const List<RSLink*>&);

		// Test membership
		bool has(const RSLink*) const;

		// Given a list of RSLinks, adds an ARSExclusionGroup to each one
		// and adds everyone to each member's set
		static void process_and_create(const List<RSLink*>&);

	protected:
		List<RSLink*> m_set;
	};

	// Attaches to a System. Contains RS Latency Queries (Link->parametername)
	class ARSLatencyQueries : public Aspect
	{
	public:
		using Query = std::pair<RSLink*, std::string>;

		void add(RSLink*, const std::string&);
		const List<Query>& queries() const;

	protected:
		List<Query> m_queries;
	};

	class RSPort : public Port
	{
	public:
		static const SigRoleID ROLE_READY;
		static const SigRoleID ROLE_VALID;
		static const SigRoleID ROLE_DATA;
		static const SigRoleID ROLE_DATA_BUNDLE;
		static const SigRoleID ROLE_LPID;
		static const SigRoleID ROLE_EOP;
		static const SigRoleID ROLE_SOP;

		static const FieldID FIELD_LPID;
		static const FieldID FIELD_DATA;

		RSPort(Dir dir);
		RSPort(Dir dir, const std::string& name);
		RSPort(const RSPort&);
		~RSPort();

		static RSPort* get_rs_port(Port*);

		TopoPort* get_topo_port() const;
		RVDPort* get_rvd_port() const;
		ClockPort* get_clock_port() const;
		List<RSLinkpoint*> get_linkpoints() const;
		PROP_GET_SET(domain_id, int, m_domain_id);
		PROP_GET_SET(clock_port_name, const std::string&, m_clk_port_name);

		void refine(NetType) override;
		HierObject* instantiate() override;

	protected:
		int m_domain_id;
		std::string m_clk_port_name;

		void refine_topo();
		void refine_rvd();
	};

	class RSLinkpoint : public Port
	{
	public:
		enum Type
		{
			INVALID,
			BROADCAST,
			UNICAST,
			MULTICAST
		};

		static Type type_from_str(const char*);

		RSLinkpoint(Dir, Type);
		~RSLinkpoint();

		PROP_GET(type, Type, m_type);
		PROP_GET_SET(encoding, const Value&, m_encoding);

		HierObject* instantiate() override;

	protected:
		Type m_type;
		Value m_encoding;
	};
}