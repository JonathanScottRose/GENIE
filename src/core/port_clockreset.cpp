#include "pch.h"
#include "port_clockreset.h"
#include "net_clockreset.h"

using namespace genie::impl;

//
// Clock
//

PortClock::PortClock(const std::string & name, genie::Port::Dir dir)
    : Port(name, dir)
{
	make_connectable(NET_CLOCK);
}

PortClock::PortClock(const PortClock &o)
    : Port(o), m_binding(o.m_binding)
{
}

Port * PortClock::instantiate() const
{
    return new PortClock(*this);
}

void PortClock::resolve_params(ParamResolver& r)
{
    m_binding.resolve_params(r);
}

//
// Reset
//

PortReset::PortReset(const std::string & name, genie::Port::Dir dir)
    : Port(name, dir)
{
	make_connectable(NET_RESET);
}

PortReset::PortReset(const PortReset &o)
    : Port(o), m_binding(o.m_binding)
{
}

Port * PortReset::instantiate() const
{
    return new PortReset(*this);
}

void PortReset::resolve_params(ParamResolver& r)
{
    m_binding.resolve_params(r);
}
