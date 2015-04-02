#pragma once

#include <forward_list>
#include "genie/expressions.h"

namespace genie
{
	namespace expressions
	{
		namespace nodes
		{
			class Node;
			class OpNode;
			class ConstNode;
			class NameNode;
			class LogNode;
			class LiteralNode;

			class Node
			{
			public:
				virtual ~Node();

				virtual bool is_constant() const = 0;
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

				bool is_constant() const;
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
				ConstNode(const int& val);
				Node* clone() const;

				bool is_constant() const;
				int get_value(const NameResolver& r);
				void set_value(const int& val);
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

				bool is_constant() const;
				int get_value(const NameResolver& r);
				std::string to_string() const;

				PROP_GET_SET(ref, std::string&, m_ref);

			protected:
				std::string m_ref;
			};

			class LogNode : public Node
			{
			public:
				LogNode();
				Node* clone() const;

				bool is_constant() const;
				int get_value(const NameResolver& r);
				std::string to_string() const;

				PROP_GET_SET(child, Node*, m_child);

			protected:
				Node* m_child;
			};

			class LiteralNode : public Node
			{
			public:
				LiteralNode();
				LiteralNode(const std::string& val);
				Node* clone() const;

				bool is_constant() const;
				int get_value(const NameResolver& r);
				void set_value(const std::string& val);
				std::string to_string() const;

			protected:
				std::string m_value;
			};
		}
	}
}
