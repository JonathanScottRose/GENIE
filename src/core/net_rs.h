#pragma once

#include "network.h"
#include "genie/link.h"

namespace genie
{
	namespace impl
	{
		class NetRSLogical;
		class NetRS;
		class NetRSField;
		class LinkRSLogical;
		class LinkRS;
		class LinkRSField;

		extern NetType NET_RS_LOGICAL;
		extern NetType NET_RS;
		extern NetType NET_RS_FIELD;

		class NetRSLogical : public Network
		{
		public:
			static void init();
			Link* create_link() const override;

		protected:
			NetRSLogical();
			~NetRSLogical() = default;
		};

		class LinkRSLogical : virtual public Link, virtual public genie::LinkRS
		{
		public:
		public:
			LinkRSLogical();
			LinkRSLogical(const LinkRSLogical&);
			~LinkRSLogical();

			Link* clone() const override;
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

		class LinkRS : virtual public Link
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

		class NetRSField : public Network
		{
		public:
			static void init();
			Link* create_link() const override;

		protected:
			NetRSField();
			~NetRSField() = default;
		};

		class LinkRSField : virtual public Link
		{
		public:
			LinkRSField();
			LinkRSField(const LinkRSField&);
			~LinkRSField();

			Link* clone() const override;
		protected:
		};
	}
}