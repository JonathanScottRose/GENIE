#pragma once

#include <string>

namespace genie
{
namespace impl
{
	class IntExpression
	{
	public:
		IntExpression();
		IntExpression(const IntExpression&);
		IntExpression(IntExpression&&);
		IntExpression(int val);
		IntExpression(const std::string&);
		~IntExpression();

		std::string to_string() const;
		int to_int() const;
		void evaluate(void* name_resolver);
		operator int() const;

	protected:
		bool m_is_const;
		union
		{
			int m_const_val;
			void* m_expr_root;
		};
	};

}
}