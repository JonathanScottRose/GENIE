#include "genie/net_clock.h"
#include "genie/genie.h"

using namespace genie;

namespace
{
	// Define network
	class NetClockDef : public NetTypeDef
	{
	public:
		NetClockDef(NetType id)
			: NetTypeDef(id)
		{
			m_name = "clock";
			m_desc = "Clock";
			m_has_port = true;
			m_src_multibind = true;
			m_sink_multibind = false;
			m_ep_asp_id = Aspect::asp_id_of<AClockEndpoint>();
		}

		~NetClockDef()
		{
		}

		Link* create_link()
		{
			return new ClockLink();
		}

		AEndpoint* create_endpoint(Dir dir, HierObject* container)
		{
			return new AClockEndpoint(dir, container);
		}

		Port* create_port(Dir dir, const std::string& name, HierObject* parent)
		{
			return new ClockPort(dir, name, parent);
		}

		PortDef* create_port_def(Dir dir, const std::string& name, HierObject* parent)
		{
			return new ClockPortDef(dir, name, parent);
		}

		Export* create_export(Dir dir, const std::string& name, System* parent)
		{
			return new ClockExport(dir, name, parent);
		}
	};

	// For port defs and exports
	template <class PD_OR_EXP>
	HierObject* pd_or_exp_instantiate(PD_OR_EXP* container)
	{
		ClockPort* result = new ClockPort(container->get_dir());
		result->set_name(container->get_name());

		// The new port is an instance of this port def or export
		result->set_prototype(container);

		return result;
	}
}

// Register the network type
const NetType genie::NET_CLOCK = NetTypeDef::add<NetClockDef>();

//
// ClockPortDef
//

ClockPortDef::ClockPortDef(Dir dir, const std::string& name, HierObject* parent)
: ClockPortDef(dir)
{
	set_name(name);
	set_parent(parent);
}

ClockPortDef::ClockPortDef(Dir dir)
: PortDef(dir)
{
}

ClockPortDef::~ClockPortDef()
{
}

HierObject* ClockPortDef::instantiate()
{
	return pd_or_exp_instantiate<ClockPortDef>(this);
}

//
// ClockPort
//

ClockPort::ClockPort(Dir dir)
: Port(dir)
{
	asp_add(new AClockEndpoint(dir, this));
}

ClockPort::ClockPort(Dir dir, const std::string& name, HierObject* parent)
: ClockPort(dir)
{
	set_name(name);
	set_parent(parent);
}

ClockPort::~ClockPort()
{
}

//
// ClockExport
//

ClockExport::ClockExport(Dir dir)
: Export(dir)
{
	asp_add(new AClockEndpoint(dir_rev(dir), this));
}

ClockExport::ClockExport(Dir dir, const std::string& name, System* parent)
: ClockExport(dir)
{
	set_name(name);
	set_parent(parent);
}

ClockExport::~ClockExport()
{
}

HierObject* ClockExport::instantiate()
{
	return pd_or_exp_instantiate<ClockExport>(this);
}

//
// AClockEndpoint
//

AClockEndpoint::AClockEndpoint(Dir dir, HierObject* container)
: AEndpoint(dir, container)
{
}

AClockEndpoint::~AClockEndpoint()
{
}

