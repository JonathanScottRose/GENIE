#include "pch.h"
#include "port_clockreset.h"

using namespace genie::impl;

//
// Clock
//

ClockPort::ClockPort(const std::string & name, genie::Port::Dir dir)
    : Port(name, dir)
{
}

ClockPort::ClockPort(const ClockPort &o)
    : Port(o), m_binding(o.m_binding)
{
}

Port * ClockPort::instantiate() const
{
    return new ClockPort(*this);
}

void ClockPort::resolve_params(ParamResolver& r)
{
    m_binding.resolve_params(r);
}

//
// Reset
//

ResetPort::ResetPort(const std::string & name, genie::Port::Dir dir)
    : Port(name, dir)
{
}

ResetPort::ResetPort(const ResetPort &o)
    : Port(o), m_binding(o.m_binding)
{
}

Port * ResetPort::instantiate() const
{
    return new ResetPort(*this);
}

void ResetPort::resolve_params(ParamResolver& r)
{
    m_binding.resolve_params(r);
}
