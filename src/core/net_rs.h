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

	public:
		LinkRSLogical();
		LinkRSLogical(const LinkRSLogical&);
		~LinkRSLogical();

		LinkRSLogical* clone() const override;
		
		PROP_GET_SET(domain_id, unsigned, m_domain_id);
		PROP_SET(src_addr, unsigned, m_src_addr);
		PROP_SET(sink_addr, unsigned, m_sink_addr);

	protected:
		unsigned m_domain_id;
		unsigned m_src_addr;
		unsigned m_sink_addr;
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

		PROP_GET_SET(latency, unsigned, m_latency);

	protected:
		LinkRSPhys(const LinkRSPhys&) = default;

		unsigned m_latency;
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