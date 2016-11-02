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

	extern NetType NET_RS;

    class NetRS : public Network
    {
    public:
        static void init();
        NetRS();
        Link* create_link() override;
        Port* create_port(Dir dir) override;
        Port* export_port(System* sys, Port* port, const std::string& name) override;
    };

	class RSLink : public Link
	{
	public:
		RSLink();

		static List<RSLink*> get_from_rvd_port(RVDPort*);
        static List<RSLink*> get_from_topo_port(TopoPort*);
		
		bool contained_in_rvd_port(RVDPort*) const;

		PROP_GET_SETR(flow_id, Value&, m_flow_id);
		PROP_GET_SET(domain_id, int, m_domain_id);
        PROP_GET_SET(latency, int, m_latency);
        PROP_GET_SET(pkt_size, int, m_pkt_size);
        PROP_GET_SET(importance, float, m_importance);

		Link* clone() const override;

	protected:
        int m_latency = 0; // for internal links
        int m_pkt_size = 0;
        float m_importance = -1; 
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
		static SigRoleID ROLE_READY;
		static SigRoleID ROLE_VALID;
		static SigRoleID ROLE_DATA;
		static SigRoleID ROLE_DATA_BUNDLE;
		static SigRoleID ROLE_LPID;
		static SigRoleID ROLE_EOP;
		static SigRoleID ROLE_SOP;

		static FieldID FIELD_LPID;
		static FieldID FIELD_DATA;

		RSPort(Dir dir);
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

    using RSPath = List<RSLink*>;

    struct RSLatencyConstraint
    {
        enum TermSign
        {
            PLUS,
            MINUS
        };

        enum CompareOp
        {
            LT,
            LEQ,
            EQ,
            GEQ,
            GT
        };

        using PathTerm = std::pair<TermSign, RSPath>;

        List<PathTerm> path_terms;
        CompareOp op;
        int rhs;
    };

    class ARSLatencyConstraints : public Aspect
    {
    public:
        List<RSLatencyConstraint> constraints;
    };

    class ATransmissionSpecs : public Aspect
    {
    public:
        unsigned pkt_size = 1;
        float importance = 1.0;
    };
}