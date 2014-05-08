#pragma once

#include "ct/common.h"

namespace ct
{
	namespace Expressions
	{
		namespace Nodes { class Node; }

		class Expression;
		typedef std::function<Expression(const std::string&)> NameResolver;

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

		protected:
			static Nodes::Node* parse(const std::string& str);

			Nodes::Node* get_root() const {	return m_root; }
			void set_root(Nodes::Node* root);

			Nodes::Node* m_root;
		};
	}
}
