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

	// TODO: use proper arbitrary-length bit vectors
	using AddressVal = uint64_t;

	class LinkRS : virtual public Link
	{
	public:
		static const AddressVal ADDR_ANY = std::numeric_limits<AddressVal>::max();

		virtual AddressVal get_src_addr() const = 0;
		virtual AddressVal get_sink_addr() const = 0;
		virtual void set_packet_size(unsigned size) = 0;
		virtual void set_importance(float imp) = 0;

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