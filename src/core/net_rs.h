#pragma once

#include "network.h"
#include "genie/link.h"

namespace genie
{
namespace impl
{
	class NetRSLogical;
	class NetRS;
	class NetRSSub;
	class LinkRSLogical;
	class LinkRS;
	class LinkRSSub;

	extern NetType NET_RS_LOGICAL;
	extern NetType NET_RS;
	extern NetType NET_RS_SUB;

	class NetRSLogical : public Network
	{
	public:
		static void init();
		Link* create_link() const override;

	protected:
		NetRSLogical();
		~NetRSLogical() = default;
	};

	class LinkRSLogical : virtual public genie::LinkRS, public Link
	{
	public:
	public:
		LinkRSLogical();
		LinkRSLogical(const LinkRSLogical&);
		~LinkRSLogical();

		Link* clone() const override;

		unsigned get_domain_id() const;

	protected:
	};

	///
	///

	class NetRS : public Network
	{
	public:
		static void init();
		Link* create_link() const override;

	protected:
		NetRS();
		~NetRS() = default;
	};

	class LinkRS : public Link
	{
	public:
		LinkRS();
		LinkRS(const LinkRS&);
		~LinkRS();

		Link* clone() const override;
	protected:
	};

	///
	///

	class NetRSSub : public Network
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