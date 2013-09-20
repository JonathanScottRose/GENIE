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

Node* OpNode::clone() const
{
	OpNode* result = new OpNode(m_op);
	result->set_lhs(m_lhs->clone());
	result->set_rhs(m_rhs->clone());
	return result;
}

std::string OpNode::to_string() const
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

Node* ConstNode::clone() const
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

std::string ConstNode::to_string() const
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

Node* NameNode::clone() const
{
	return new NameNode(*this);
}

int NameNode::get_value(const NameResolver& r)
{
	const Expression* expr = r(m_ref);
	return expr->get_value(r);
}

std::string NameNode::to_string() const
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
	: m_root(nullptr)
{
	*this = other;
}

Expression::Expression(const std::string& str)
	: m_root(nullptr)
{
	*this = str;
}

Expression::Expression(int val)
	: m_root(nullptr)
{
	*this = val;
}

Expression::~Expression()
{
	delete m_root;
}

std::string Expression::to_string() const
{
	return m_root->to_string();
}

void Expression::set_root(Node* root)
{
	if (m_root) delete m_root;
	m_root = root;
}

int Expression::get_value(const NameResolver& r) const
{
	return m_root->get_value(r);
}

int Expression::get_const_value() const
{
	static NameResolver r = [] (const std::string& name)
	{
		assert(false);
		return nullptr;
	};

	return get_value(r);
}

Expression& Expression::operator= (const Expression& other)
{
	if (other.get_root() == nullptr)
	{
		set_root(nullptr);
	}
	else
	{
		set_root(other.get_root()->clone());
	}
	return *this;
}

Expression& Expression::operator= (int val)
{
	set_root(new ConstNode(val));
	return *this;
}

Expression& Expression::operator= (const std::string& str)
{
	set_root(build_root(str));
	return *this;
}

Expression* Expression::build(const std::string& str)
{
	Expression* result = new Expression();
	result->set_root(build_root(str));
	return result;
}

Node* Expression::build_root(const std::string& str)
{
	std::string s = str;
	std::smatch mr;
	std::regex regex("\\s*(\\d+)|([+-/*])|([[:alpha:]]+)");

	Node* node = nullptr;
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
		else
		{
			assert(false);
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
	return node;
}