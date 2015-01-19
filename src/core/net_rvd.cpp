#include "genie/net_Rvd.h"
#include "genie/genie.h"

using namespace genie;

namespace
{
	// Define network
	class NetRVDDef : public NetTypeDef
	{
	public:
		NetRVDDef(NetType id)
			: NetTypeDef(id)
		{
			m_name = "rvd";
			m_desc = "Point-to-Point Ready/Valid/Data";
			m_has_port = true;
			m_src_multibind = false;
			m_sink_multibind = false;
			m_ep_asp_id = Aspect::asp_id_of<ARVDEndpoint>();
		}

		~NetRVDDef()
		{
		}

		Link* create_link()
		{
			return new RVDLink();
		}

		AEndpoint* create_endpoint(Dir dir, HierObject* container)
		{
			return new ARVDEndpoint(dir, container);
		}

		Port* create_port(Dir dir, const std::string& name, HierObject* parent)
		{
			return new RVDPort(dir, name, parent);
		}

		PortDef* create_port_def(Dir dir, const std::string& name, HierObject* parent)
		{
			return new RVDPortDef(dir, name, parent);
		}

		Export* create_export(Dir dir, const std::string& name, System* parent)
		{
			return new RVDExport(dir, name, parent);
		}
	};

	// For port defs and exports
	template <class PD_OR_EXP>
	HierObject* pd_or_exp_instantiate(PD_OR_EXP* container)
	{
		RVDPort* result = new RVDPort(container->get_dir());
		result->set_name(container->get_name());

		// The new port is an instance of this port def or export
		result->set_prototype(container);

		return result;
	}
}

// Register the network type
const NetType genie::NET_RVD = NetTypeDef::add<NetRVDDef>();

//
// RVDPortDef
//

RVDPortDef::RVDPortDef(Dir dir, const std::string& name, HierObject* parent)
: RVDPortDef(dir)
{
	set_name(name);
	set_parent(parent);
}

RVDPortDef::RVDPortDef(Dir dir)
: PortDef(dir)
{
}

HierObject* RVDPortDef::instantiate()
{
	return pd_or_exp_instantiate<RVDPortDef>(this);
}

RVDPortDef::~RVDPortDef()
{
}

//
// RVDPort
//

RVDPort::RVDPort(Dir dir)
: Port(dir)
{
	asp_add(new ARVDEndpoint(dir, this));
}

RVDPort::RVDPort(Dir dir, const std::string& name, HierObject* parent)
: RVDPort(dir)
{
	set_name(name);
	set_parent(parent);
}

RVDPort::~RVDPort()
{
}

//
// RVDExport
//

RVDExport::RVDExport(Dir dir)
: Export(dir)
{
	asp_add(new ARVDEndpoint(dir_rev(dir), this));
}

RVDExport::RVDExport(Dir dir, const std::string& name, System* parent)
: RVDExport(dir)
{
	set_name(name);
	set_parent(parent);
}

RVDExport::~RVDExport()
{
}

HierObject* RVDExport::instantiate()
{
	return pd_or_exp_instantiate<RVDExport>(this);
}

//
// ARVDEndpoint
//

ARVDEndpoint::ARVDEndpoint(Dir dir, HierObject* container)
: AEndpoint(dir, container)
{
}

ARVDEndpoint::~ARVDEndpoint()
{
}

