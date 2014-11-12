#include "genie/net_reset.h"
#include "genie/genie.h"

using namespace genie;

namespace
{
	// Define network
	class NetResetDef : public NetTypeDef
	{
	public:
		NetResetDef(NetType id)
			: NetTypeDef(id)
		{
			m_name = "reset";
			m_desc = "Reset";
			m_has_port = true;
			m_src_multibind = true;
			m_sink_multibind = false;
			m_ep_asp_id = Aspect::asp_id_of<AResetEndpoint>();
		}

		~NetResetDef()
		{
		}

		Link* create_link()
		{
			return new ResetLink();
		}

		AEndpoint* create_endpoint(Dir dir, HierObject* container)
		{
			return new AResetEndpoint(dir, container);
		}

		Port* create_port(Dir dir, const std::string& name, HierObject* parent)
		{
			return new ResetPort(dir, name, parent);
		}

		PortDef* create_port_def(Dir dir, const std::string& name, HierObject* parent)
		{
			return new ResetPortDef(dir, name, parent);
		}

		Export* create_export(Dir dir, const std::string& name, System* parent)
		{
			return new ResetExport(dir, name, parent);
		}
	};

	// For port defs and exports
	template <class PD_OR_EXP>
	class AResetInstantiable : public AspectMakeRef<AInstantiable, PD_OR_EXP>
	{
	public:
		AResetInstantiable(PD_OR_EXP* container) : AspectMakeRef(container) { }
		~AResetInstantiable() = default;

		HierObject* instantiate()
		{
			PD_OR_EXP* container = asp_container();

			ResetPort* result = new ResetPort(container->get_dir());
			result->set_name(container->get_name());

			// The new port is an instance of this port def
			result->asp_add(new AInstance(container));

			return result;
		}
	};
}

// Register the network type
const NetType genie::NET_RESET = NetTypeDef::add<NetResetDef>();

//
// ResetPortDef
//

ResetPortDef::ResetPortDef(Dir dir, const std::string& name, HierObject* parent)
: ResetPortDef(dir)
{
	set_name(name);
	set_parent(parent);
}

ResetPortDef::ResetPortDef(Dir dir)
: PortDef(dir)
{
	asp_add<AInstantiable>(new AResetInstantiable<ResetPortDef>(this));
}

ResetPortDef::~ResetPortDef()
{
}

//
// ResetPort
//

ResetPort::ResetPort(Dir dir)
: Port(dir)
{
	asp_add(new AResetEndpoint(dir, this));
}

ResetPort::ResetPort(Dir dir, const std::string& name, HierObject* parent)
: ResetPort(dir)
{
	set_name(name);
	set_parent(parent);
}

ResetPort::~ResetPort()
{
}

//
// ResetExport
//

ResetExport::ResetExport(Dir dir)
: Export(dir)
{
	asp_add(new AResetEndpoint(dir_rev(dir), this));
	asp_add<AInstantiable>(new AResetInstantiable<Export>(this));
}

ResetExport::ResetExport(Dir dir, const std::string& name, System* parent)
: ResetExport(dir)
{
	set_name(name);
	set_parent(parent);
}

ResetExport::~ResetExport()
{
}

//
// AResetEndpoint
//

AResetEndpoint::AResetEndpoint(Dir dir, HierObject* container)
: AEndpoint(dir, container)
{
}

AResetEndpoint::~AResetEndpoint()
{
}

