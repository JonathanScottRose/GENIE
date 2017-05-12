#include "pch.h"
#include "port_clockreset.h"
#include "net_clockreset.h"
#include "node_system.h"
#include "genie_priv.h"

using namespace genie::impl;

namespace
{
	class PortClockInfo : public PortTypeDef
	{
	public:
		PortClockInfo()
		{
			m_short_name = "clock";
			m_default_network = NET_CLOCK;
		}

		~PortClockInfo() = default;

		Port* create_port(const std::string& name, genie::Port::Dir dir) override
		{
			return new PortClock(name, dir);
		}
	};

	class PortResetInfo : public PortTypeDef
	{
	public:
		PortResetInfo()
		{
			m_short_name = "reset";
			m_default_network = NET_RESET;
		}

		~PortResetInfo() = default;

		Port* create_port(const std::string& name, genie::Port::Dir dir) override
		{
			return new PortReset(name, dir);
		}
	};
}

//
// Clock
//

PortType genie::impl::PORT_CLOCK;

void PortClock::init()
{
	PORT_CLOCK = genie::impl::register_port_type(new PortClockInfo());
}

PortClock::PortClock(const std::string & name, genie::Port::Dir dir)
    : Port(name, dir)
{
	make_connectable(NET_CLOCK);
}

PortClock::PortClock(const std::string & name, genie::Port::Dir dir, 
	const hdl::PortBindingRef & bnd)
	: Port(name, dir), m_binding(bnd)
{
	make_connectable(NET_CLOCK);
}

PortClock::PortClock(const PortClock &o)
    : Port(o), m_binding(o.m_binding)
{
}

PortClock * PortClock::clone() const
{
    return new PortClock(*this);
}

void PortClock::resolve_params(ParamResolver& r)
{
    m_binding.resolve_params(r);
}

Port* PortClock::export_port(const std::string& name, NodeSystem* context)
{
	auto result = context->create_clock_port(name, get_dir(), name);
	return dynamic_cast<Port*>(result);
}

PortClock * PortClock::get_connected_clk_port(Node* context) const
{
	assert(context->is_parent_of(this));

	auto ep = get_endpoint(NET_CLOCK, get_effective_dir(context));
	if (!ep->is_connected())
		return nullptr;

	auto remote = static_cast<PortClock*>(ep->get_remote_obj0());
	return remote;
}

PortClock * PortClock::get_driver(Node * context) const
{
	assert(get_dir() == Dir::IN); // flexibility for clock sources TODO
	if (get_parent() == context)
	{
		return const_cast<PortClock*>(this);
	}
	else
	{
		auto ep = get_endpoint(NET_CLOCK, get_effective_dir(context));
		if (!ep->is_connected())
			return nullptr;

		auto remote = static_cast<PortClock*>(ep->get_remote_obj0());
		return remote;
	}
}

//
// Reset
//

PortType genie::impl::PORT_RESET;

void PortReset::init()
{
	PORT_RESET = genie::impl::register_port_type(new PortResetInfo());
}

PortReset::PortReset(const std::string & name, genie::Port::Dir dir)
    : Port(name, dir)
{
	make_connectable(NET_RESET);
}

PortReset::PortReset(const std::string & name, genie::Port::Dir dir, 
	const hdl::PortBindingRef & bnd)
	: Port(name, dir), m_binding(bnd)
{
	make_connectable(NET_RESET);
}

PortReset::PortReset(const PortReset &o)
    : Port(o), m_binding(o.m_binding)
{
}

PortReset * PortReset::clone() const
{
    return new PortReset(*this);
}

void PortReset::resolve_params(ParamResolver& r)
{
    m_binding.resolve_params(r);
}

Port* PortReset::export_port(const std::string& name, NodeSystem* context)
{
	auto result = context->create_reset_port(name, get_dir(), name);
	return dynamic_cast<Port*>(result);
}
