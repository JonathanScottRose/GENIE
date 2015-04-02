#pragma once

#include "genie/common.h"
#include "genie/expressions.h"

namespace genie
{
	using expressions::Expression;

	class ParamBinding
	{
	public:
		ParamBinding(const std::string& name)
			: m_name(name), m_is_bound(false) { }

		ParamBinding(const std::string& name, const Expression& val)
			: m_name(name), m_is_bound(true), m_expr(val) { }

		PROP_GET(name, std::string&, m_name);
		PROP_GET(expr, Expression&, m_expr);

		bool is_bound() const { return m_is_bound; };
		void set_expr(const Expression& expr) { m_expr = expr; m_is_bound = true; }

	protected:
		std::string m_name;
		bool m_is_bound;
		Expression m_expr;
	};
}