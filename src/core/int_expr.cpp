#include "pch.h"
#include "genie/genie.h"
#include "int_expr.h"
#include "int_expr_nodes.h"
#include "params.h"

using namespace genie::impl;

IntExpr::IntExpr()
    : m_is_const(true), m_const_val(0)
{
}

IntExpr::IntExpr(const IntExpr& o)
    : IntExpr()
{
    *this = o;    
}

IntExpr::IntExpr(IntExpr&& o)
    : IntExpr()
{
    *this = std::move(o);    
}

IntExpr::IntExpr(const std::string& str)
    : IntExpr(str.c_str())
{
}

IntExpr::IntExpr(const char* str)
	: m_is_const(false)
{
	m_expr_root = expr::parse(str);
}

IntExpr::IntExpr(int i)
    : m_is_const(true), m_const_val(i)
{
}

IntExpr::~IntExpr()
{
    if (!m_is_const)
    {
        delete m_expr_root;
    }
}

IntExpr& IntExpr::operator=(const IntExpr& o)
{
    if (!m_is_const)
        delete m_expr_root;

    m_is_const = o.m_is_const;

    if (m_is_const)
    {
        m_const_val = o.m_const_val;
    }
    else
    {
        m_expr_root = o.m_expr_root->clone();
    }

    return *this;
}

IntExpr& IntExpr::operator=(IntExpr&& o)
{
    if (!m_is_const)
        delete m_expr_root;

    m_is_const = o.m_is_const;

    if (m_is_const)
    {
        m_const_val = o.m_const_val;
    }
    else
    {
        m_expr_root = o.m_expr_root;
        o.m_is_const = true;
        o.m_const_val = 0;
    }

    return *this;
}

bool IntExpr::equals(const IntExpr &o) const
{
    if (m_is_const && o.m_is_const)
    {
        return m_const_val == o.m_const_val;
    }
    else if (!m_is_const && !o.m_is_const &&
        m_expr_root && o.m_expr_root)
    {
        return m_expr_root->to_string() == o.m_expr_root->to_string();
    }

	return false;
}

IntExpr::operator int() const
{
    if (!m_is_const)
    {
        throw Exception("IntExpr has not been evaluated yet");
    }

    return m_const_val;
}

std::string IntExpr::to_string() const
{
    if (m_is_const)
    {
        return std::to_string(m_const_val);
    }
    else
    {
        assert(m_expr_root);
        return m_expr_root->to_string();
    }
}

int IntExpr::evaluate(ParamResolver& r)
{
    // Evaluate if not already done so
    if (!m_is_const)
    {
        assert(m_expr_root);
        // Save to temporary first! m_const_val and m_expr_root share memory
        int const_val = m_expr_root->evaluate(r);
        delete m_expr_root;
        m_const_val = const_val;
        m_is_const = true;
    }

    return m_const_val;
}


