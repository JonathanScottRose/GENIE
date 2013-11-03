#pragma once

#include "expressions.h"

namespace ct
{
	namespace Expressions
	{
		namespace Nodes
		{
			class Node;
			class OpNode;
			class SumNode;
			class ProductNode;
			class ConstNode;
			class NameNode;

			class Node
			{
			public:
				virtual ~Node();

				virtual int get_value(const NameResolver& r) = 0;
				virtual Node* clone() const = 0;
				virtual std::string to_string() const = 0;
			};

			class OpNode : public Node
			{
			public:
				typedef std::forward_list<Node*> Children;

				OpNode();
				OpNode(char op);
				virtual ~OpNode();

				void clear();
				PROP_GET(lhs, Node*, m_lhs);
				PROP_GET(rhs, Node*, m_rhs);
				void set_lhs(Node* node);
				void set_rhs(Node* node);

				int get_value(const NameResolver& r);
				Node* clone() const;
				std::string to_string() const;

			protected:
				char m_op;
				Node* m_lhs;
				Node* m_rhs;
			};

			class ConstNode : public Node
			{
			public:
				ConstNode();
				ConstNode(int val);
				Node* clone() const;

				int get_value(const NameResolver& r);
				void set_value(int val);
				std::string to_string() const;

			protected:
				int m_value;
			};

			class NameNode : public Node
			{
			public:
				NameNode();
				NameNode(const std::string& expr_name);
				Node* clone() const;

				int get_value(const NameResolver& r);
				std::string to_string() const;

				PROP_GET_SET(ref, const std::string&, m_ref);

			protected:
				std::string m_ref;
			};
		}
	}
}