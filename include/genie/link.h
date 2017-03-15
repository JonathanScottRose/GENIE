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
}