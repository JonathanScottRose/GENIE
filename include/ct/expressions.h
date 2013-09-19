#pragma once

#include "common.h"

namespace ct
{
namespace Expressions
{

class Expression;
class Node;
class OpNode;
class SumNode;
class ProductNode;
class ConstNode;
class NameNode;

typedef std::function<Expression*(const std::string&)> NameResolver;

class Node
{
public:
	virtual ~Node();

	virtual int get_value(const NameResolver& r) = 0;
	virtual Node* clone() = 0;
	virtual std::string to_string() = 0;
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
	Node* clone();
	std::string to_string();

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
	Node* clone();

	int get_value(const NameResolver& r);
	void set_value(int val);
	std::string to_string();

protected:
	int m_value;
};

class NameNode : public Node
{
public:
	NameNode();
	NameNode(const std::string& expr_name);
	Node* clone();

	int get_value(const NameResolver& r);
	std::string to_string();

	PROP_GET_SET(ref, const std::string&, m_ref);

protected:
	std::string m_ref;
};

class Expression
{
public:
	Expression();
	Expression(const Expression&);
	~Expression();

	int get_value(const NameResolver& r);
	std::string to_string();
	
	Node* get_root() { return m_root; }
	void set_root(Node* root);

	Expression& operator= (const std::string& str);
	static Expression* build(const std::string& str);

protected:
	Node* m_root;
};


}
}