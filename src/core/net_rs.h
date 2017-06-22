#pragma once

#include "prop_macros.h"
#include "network.h"
#include "genie/link.h"

namespace genie
{
namespace impl
{
	class NetRSLogical;
	class NetRSPhys;
	class NetRSSub;
	class LinkRSLogical;
	class LinkRSPhys;
	class LinkRSSub;

	extern NetType NET_RS_LOGICAL;
	extern NetType NET_RS_PHYS;
	extern NetType NET_RS_SUB;

	class NetRSLogical : public NetworkDef
	{
	public:
		static void init();
		Link* create_link() const override;

	protected:
		NetRSLogical();
		~NetRSLogical() = default;
	};

	class LinkRSLogical : public genie::LinkRS, public Link
	{
	public:
		unsigned get_src_addr() const override;
		unsigned get_sink_addr() const override;
		void set_packet_size(unsigned) override;
		void set_importance(float) override;

	public:
		static constexpr unsigned PKT_SIZE_UNSET = std::numeric_limits<unsigned>::max();
		static constexpr float IMPORTANCE_UNSET = -1;

		LinkRSLogical();
		LinkRSLogical(const LinkRSLogical&) = default;
		~LinkRSLogical();

		LinkRSLogical* clone() const override;
		
		PROP_GET_SET(domain_id, unsigned, m_domain_id);
		PROP_SET(src_addr, unsigned, m_src_addr);
		PROP_SET(sink_addr, unsigned, m_sink_addr);
		PROP_GET(packet_size, unsigned, m_pkt_size);
		PROP_GET(importance, float, m_importance);

	protected:
		unsigned m_domain_id;
		unsigned m_src_addr;
		unsigned m_sink_addr;
		unsigned m_pkt_size;
		float m_importance;
	};

	///
	///

	class NetRSPhys : public NetworkDef
	{
	public:
		static void init();
		Link* create_link() const override;

	protected:
		NetRSPhys();
		~NetRSPhys() = default;
	};

	class LinkRSPhys : public Link
	{
	public:
		LinkRSPhys();
		~LinkRSPhys();

		Link* clone() const override;

		unsigned get_latency() const;
		void set_latency(unsigned);

		unsigned get_logic_depth() const;
		void set_logic_depth(unsigned);

	protected:
		LinkRSPhys(const LinkRSPhys&) = default;

		// >0 : number of register levels (latency)
		// <=0 : combinational depth in LUTs
		int m_lat_or_comb;
	};

	///
	///

	class NetRSSub : public NetworkDef
	{
	public:
		static void init();
		Link* create_link() const override;

	protected:
		NetRSSub();
		~NetRSSub() = default;
	};

	class LinkRSSub : public Link
	{
	public:
		LinkRSSub();
		LinkRSSub(const LinkRSSub&);
		~LinkRSSub();

		Link* clone() const override;
	protected:
	};
}
}