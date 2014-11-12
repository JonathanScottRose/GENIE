#pragma once

#include "genie/networks.h"
#include "genie/structure.h"

namespace genie
{
	// Globally-accessible network type ID. Maybe move to a static member?
	extern const NetType NET_RVD;

	class ARVDEndpoint : public AEndpoint
	{
	public:
		ARVDEndpoint(Dir, HierObject*);
		~ARVDEndpoint();

		NetType get_type() const { return NET_RVD; }
	};

	// RVD Link : no special customization needed
	typedef Link RVDLink;

	class RVDPortDef : public PortDef
	{
	public:
		RVDPortDef(Dir dir);
		RVDPortDef(Dir dir, const std::string& name, HierObject* parent);
		~RVDPortDef();

		NetType get_type() const { return NET_RVD; }
	};

	class RVDPort : public Port
	{
	public:
		RVDPort(Dir dir);
		RVDPort(Dir dir, const std::string& name, HierObject* parent = nullptr);
		~RVDPort();

		NetType get_type() const { return NET_RVD; }
	};

	class RVDExport : public Export
	{
	public:
		RVDExport(Dir dir);
		RVDExport(Dir dir, const std::string& name, System* parent);
		~RVDExport();

		NetType get_type() const { return NET_RVD; }
	};
}