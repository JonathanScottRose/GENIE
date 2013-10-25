#pragma once

#include "common.h"

namespace ct
{
	namespace Expressions
	{
		namespace Nodes { class Node; }

		class Expression;
		typedef std::function<const Expression*(const std::string&)> NameResolver;

		class Expression
		{
		public:
			Expression();
			Expression(const Expression&);
			Expression(const std::string&);
			Expression(const char*);
			Expression(int);
			~Expression();

			int get_value(const NameResolver& r) const;
			int get_const_value() const;
			std::string to_string() const;

			Nodes::Node* get_root() const { return m_root; }
			void set_root(Nodes::Node* root);

			Expression& operator= (const Expression& other);
			Expression& operator= (const std::string& str);
			Expression& operator= (int val);

			static Expression* build(const std::string& str);

		protected:
			static Nodes::Node* build_root(const std::string& str);
			Nodes::Node* m_root;
		};
	}
}