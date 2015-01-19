#pragma once

#include "genie/networks.h"
#include "genie/structure.h"

namespace genie
{
	// Globally-accessible network type ID. Maybe move to a static member?
	extern const NetType NET_CLOCK;

	class AClockEndpoint : public AEndpoint
	{
	public:
		AClockEndpoint(Dir, HierObject*);
		~AClockEndpoint();

		NetType get_type() const { return NET_CLOCK; }
	};

	// Clock Link : no special customization needed
	typedef Link ClockLink;
	
	class ClockPortDef : public PortDef
	{
	public:
		ClockPortDef(Dir dir);
		ClockPortDef(Dir dir, const std::string& name, HierObject* parent);
		~ClockPortDef();

		HierObject* instantiate();

		NetType get_type() const { return NET_CLOCK; }
	};

	class ClockPort : public Port
	{
	public:
		ClockPort(Dir dir);
		ClockPort(Dir dir, const std::string& name, HierObject* parent = nullptr);
		~ClockPort();

		NetType get_type() const { return NET_CLOCK; }
	};

	class ClockExport : public Export
	{
	public:
		ClockExport(Dir dir);
		ClockExport(Dir dir, const std::string& name, System* parent);
		~ClockExport();

		HierObject* instantiate();

		NetType get_type() const { return NET_CLOCK; }
	};
}