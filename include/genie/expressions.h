#pragma once

#include "genie/common.h"

namespace genie
{
	namespace expressions
	{
		namespace nodes { class Node; }

		class Expression;
		typedef std::function<int(const std::string&)> NameResolver;

		class Expression
		{
		public:
			Expression();
			Expression(const Expression&);
			Expression(Expression&&);
			Expression(const std::string&);
			Expression(const char*);
			Expression(int);
			~Expression();

			Expression& operator= (const Expression& other);
			Expression& operator= (Expression&& other);

			bool is_const() const;
			int get_value(const NameResolver& r = get_const_resolver()) const;
			std::string to_string() const;
			operator std::string() const;

			static const NameResolver& get_const_resolver();
			static Expression build_hack_expression(const std::string&);

		protected:
			static nodes::Node* parse(const std::string& str);

			nodes::Node* get_root() const {	return m_root; }
			void set_root(nodes::Node* root);

			nodes::Node* m_root;
		};
	}
}