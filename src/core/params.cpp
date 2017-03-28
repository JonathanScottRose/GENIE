#include "pch.h"
#include "params.h"

using namespace genie::impl;

//
// String param
//

NodeStringParam::NodeStringParam(const std::string & s)
    : m_str(s)
{
}

NodeStringParam::NodeStringParam(std::string && s)
    : m_str(std::move(s))
{
}

//
// Literal param
//

NodeLiteralParam::NodeLiteralParam(const std::string & s)
    : m_str(s)
{
}

NodeLiteralParam::NodeLiteralParam(std::string && s)
    : m_str(std::move(s))
{
}

//
// Int param
//

NodeIntParam::NodeIntParam(int i)
    : m_int(i)
{
}

NodeIntParam::NodeIntParam(const IntExpr& exp)
    : m_int(exp)
{
}

NodeIntParam::NodeIntParam(IntExpr&& exp)
    : m_int(std::move(exp))
{
}

NodeIntParam::NodeIntParam(const std::string& s)
    : m_int(s)
{
}

void NodeIntParam::resolve(ParamResolver& r)
{
    m_int.evaluate(r);
}

bool NodeIntParam::is_resolved() const
{
	return m_int.is_const();
}

void NodeIntParam::set_val(const IntExpr& exp)
{
    m_int = exp;
}

void NodeIntParam::set_val(IntExpr&& exp)
{
    m_int = std::move(exp);
}

//
// Bits param
//

void NodeBitsParam::resolve(ParamResolver &)
{
}

void NodeBitsParam::set_val(const BitsVal& o)
{
    m_val = o;
}

void NodeBitsParam::set_val(BitsVal&& o)
{
    m_val = std::move(o);
}
