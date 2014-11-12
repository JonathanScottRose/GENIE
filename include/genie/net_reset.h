#pragma once

#include "genie/networks.h"
#include "genie/structure.h"

namespace genie
{
	extern const NetType NET_RESET;

	class AResetEndpoint : public AEndpoint
	{
	public:
		AResetEndpoint(Dir, HierObject*);
		~AResetEndpoint();

		NetType get_type() const {
			return NET_RESET;
		}
	};

	// Reset Link : no special customization needed
	typedef Link ResetLink;

	class ResetPortDef : public PortDef
	{
	public:
		ResetPortDef(Dir dir);
		ResetPortDef(Dir dir, const std::string& name, HierObject* parent);
		~ResetPortDef();

		NetType get_type() const {
			return NET_RESET;
		}
	};

	class ResetPort : public Port
	{
	public:
		ResetPort(Dir dir);
		ResetPort(Dir dir, const std::string& name, HierObject* parent = nullptr);
		~ResetPort();

		NetType get_type() const {
			return NET_RESET;
		}
	};

	class ResetExport : public Export
	{
	public:
		ResetExport(Dir dir);
		ResetExport(Dir dir, const std::string& name, System* parent);
		~ResetExport();

		NetType get_type() const {
			return NET_RESET;
		}
	};
}