#pragma once

#include "genie.h"

namespace genie
{
	class Link : virtual public APIObject
	{
	public:
	protected:
		~Link() = default;
	};

	class LinkRS : virtual public Link
	{
	public:
		static const unsigned ADDR_ANY = std::numeric_limits<unsigned>::max();

		virtual unsigned get_src_addr() const = 0;
		virtual unsigned get_sink_addr() const = 0;

	protected:
		~LinkRS() = default;
	};

	class LinkTopo : virtual public Link
	{
	public:
		static const unsigned REGS_UNLIMITED = std::numeric_limits<unsigned>::max();

		virtual void set_minmax_regs(unsigned min, unsigned max = REGS_UNLIMITED) = 0;

	protected:
		~LinkTopo() = default;
	};
}