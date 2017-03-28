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
	protected:
		~LinkRS() = default;
	};

	class LinkTopo : virtual public Link
	{
	public:
		virtual void set_min_regs(unsigned) = 0;

	protected:
		~LinkTopo() = default;
	};
}