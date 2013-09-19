#include <regex>
#include "expressions.h"

using namespace ct;
using namespace ct::Expressions;

//
// Node
//

Node::~Node()
{
}

//
// OpNode
//

OpNode::OpNode()
	: m_op(0), m_lhs(nullptr), m_rhs(nullptr)
{
}

OpNode::OpNode(char op)
	: m_op(op), m_lhs(nullptr), m_rhs(nullptr)
{
}

OpNode::~OpNode()
{
	clear();
}

void OpNode::clear()
{
	delete m_lhs;
	delete m_rhs;
	m_lhs = nullptr;
	m_rhs = nullptr;
}

int OpNode::get_value(const NameResolver& r)
{
	int lhs = m_lhs->get_value(r);
	int rhs = m_rhs->get_value(r);

	switch (m_op)
	{
	case '+' : return lhs + rhs;
	case '-' : return lhs - rhs;
	case '*' : return lhs * rhs;
	case '/' : return lhs / rhs;
	}

	assert(false);
	return -1;
}

void OpNode::set_lhs(Node* node)
{
	if (m_lhs) delete m_lhs;
	m_lhs = node;
}

void OpNode::set_rhs(Node* node)
{
	if (m_rhs) delete m_rhs;
	m_rhs = node;
}

Node* OpNode::clone()
{
	OpNode* result = new OpNode(m_op);
	result->set_lhs(m_lhs->clone());
	result->set_rhs(m_rhs->clone());
	return result;
}

std::string OpNode::to_string()
{
	std::string result = m_lhs->to_string();
	result.append(1, m_op);
	result.append(m_rhs->to_string());
	return result;
}

//
// ConstNode
//

ConstNode::ConstNode()
{
}

ConstNode::ConstNode(int val)
	: m_value(val)
{
}

Node* ConstNode::clone()
{
	return new ConstNode(*this);
}

int ConstNode::get_value(const NameResolver& r)
{
	return m_value;
}

void ConstNode::set_value(int val)
{
	m_value = val;
}

std::string ConstNode::to_string()
{
	return std::to_string(m_value);
}

//
// NameNode
//

NameNode::NameNode()
{
}

NameNode::NameNode(const std::string& name)
	: m_ref(name)
{
}

Node* NameNode::clone()
{
	return new NameNode(*this);
}

int NameNode::get_value(const NameResolver& r)
{
	Expression* expr = r(m_ref);
	return expr->get_value(r);
}

std::string NameNode::to_string()
{
	return m_ref;
}

//
// Expression
//

Expression::Expression()
	: m_root(nullptr)
{
}

Expression::Expression(const Expression& other)
{
	m_root = other.m_root->clone();
}

Expression::~Expression()
{
	delete m_root;
}

std::string Expression::to_string()
{
	return m_root->to_string();
}

void Expression::set_root(Node* root)
{
	if (m_root) delete m_root;
	m_root = root;
}

int Expression::get_value(const NameResolver& r)
{
	return m_root->get_value(r);
}

Expression* Expression::build(const std::string& str)
{
	Expression* result = new Expression();

	std::string s = str;
	std::smatch mr;
	std::regex regex("\\s*(\\d+)|([+-/*])|([[:alpha:]]+)\\s");

	Node* node;
	Node* last = nullptr;
	char op = 0;
	bool has_op = false;

	while (std::regex_search(s, mr, regex))
	{
		node = nullptr;

		if (mr[1].matched)
		{
			node = new ConstNode(std::stoi(mr[1]));
		}
		else if (mr[2].matched)
		{
			has_op = true;
			op = mr[2].str().at(0);
		}
		else if (mr[3].matched)
		{
			node = new NameNode(mr[3]);
		}

		if (node)
		{
			if (has_op)
			{
				assert(last);
				assert(op);

				OpNode* op_node = new OpNode(op);
				op_node->set_lhs(last);
				op_node->set_rhs(node);
				node = op_node;

				has_op = false;
				op = 0;
			}

			last = node;
		}

		s = mr.suffix().str();
	}

	assert(node);
	result->set_root(node);

	return result;
}