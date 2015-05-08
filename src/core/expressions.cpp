#include <stack>
#include <sstream>
#include "genie/expressions.h"
#include "genie/regex.h"
#include "expressions_nodes.h"

using namespace genie;
using namespace genie::expressions;
using namespace genie::expressions::nodes;

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

bool OpNode::is_constant() const
{
	return (m_lhs->is_constant() && m_rhs->is_constant());
}

int OpNode::get_value(const NameResolver& r)
{
	int lhs = m_lhs->get_value(r);
	int rhs = m_rhs->get_value(r);

	switch (m_op)
	{
		case '+': return lhs + rhs;
		case '-': return lhs - rhs;
		case '*': return lhs * rhs;
		case '/': return lhs / rhs;
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

ConstNode::ConstNode(const int& val)
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

void ConstNode::set_value(const int& val)
{
	m_value = val;
}

std::string ConstNode::to_string() const
{
	return std::to_string(m_value);
}

bool ConstNode::is_constant() const
{
	return true;
}

//
// LiteralNode
//

LiteralNode::LiteralNode()
{
}

LiteralNode::LiteralNode(const std::string& val)
: m_value(val)
{
}

Node* LiteralNode::clone() const
{
	return new LiteralNode(*this);
}

int LiteralNode::get_value(const NameResolver& r)
{
	throw Exception("NO!");
	assert(false);
	return -1;
}

void LiteralNode::set_value(const std::string& val)
{
	m_value = val;
}

std::string LiteralNode::to_string() const
{
	return m_value;
}

bool LiteralNode::is_constant() const
{
	return true;
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
	return r(m_ref);
}

std::string NameNode::to_string() const
{
	return m_ref;
}

bool NameNode::is_constant() const
{
	return false;
}

//
// LogNode
//

LogNode::LogNode()
{
}

Node* LogNode::clone() const
{
	return new LogNode(*this);
}

int LogNode::get_value(const NameResolver& r)
{
	int v = m_child->get_value(r);
	return util::log2(v);
}

std::string LogNode::to_string() const
{
	return "$clog2(" + m_child->to_string() + ")";
}

bool LogNode::is_constant() const
{
	return m_child->is_constant();
}



//
// Expression
//

Expression::Expression()
: m_root(nullptr)
{
}

Expression::Expression(const Expression& other)
: Expression()
{
	*this = other;
}

Expression::Expression(Expression&& other)
{
	// c++11 Move Constructor. Efficient!
	m_root = other.m_root;
	other.m_root = nullptr;
}

Expression::Expression(const std::string& str)
: Expression()
{
	set_root(parse(str));
}

Expression::Expression(int val)
: Expression()
{
	set_root(new ConstNode(val));
}

Expression::Expression(const char* str)
: Expression(std::string(str))
{
}

Expression::~Expression()
{
	delete m_root;
}

std::string Expression::to_string() const
{
	return m_root->to_string();
}

Expression::operator std::string() const
{
	return to_string();
}

bool Expression::is_const() const
{
	return m_root->is_constant();
}

void Expression::set_root(Node* root)
{
	if (m_root) delete m_root;
	m_root = root;
}

int Expression::get_value(const NameResolver& r) const
{
	if (m_root == nullptr)
		throw Exception("Empty expression");

	return m_root->get_value(r);
}

auto Expression::get_const_resolver() -> const NameResolver&
{
	static NameResolver s_null_resolv = 
		[](const std::string& name)
		{
			throw Exception("Subexpression not constant: " + name);
			return 0;
		};

	return s_null_resolv;
}

Expression& Expression::operator= (const Expression& other)
{
	Node* other_root = other.get_root();

	if (other_root == nullptr)
	{
		set_root(nullptr);
	}
	else if (other_root != m_root) // self-check
	{
		set_root(other_root->clone());
	}
	return *this;
}

Expression& Expression::operator= (Expression&& other)
{
	Node* other_root = other.get_root();

	if (other_root == nullptr)
	{
		set_root(nullptr);
	}
	else if (other_root != m_root) // self-check
	{
		set_root(other_root);
		// do NOT use other.set_root(nullptr) here or else it will delete
		// the root before nulling it
		other.m_root = nullptr;
	}
	return *this;
}

Node* Expression::parse(const std::string& str)
{
	std::string s = str;
	std::smatch mr;
	std::regex regex("\\s*(\\d+)|([+-/*])|(%)|([[:alnum:]_]+)");

	Node* node = nullptr;
	Node* last = nullptr;
	char op = 0;
	bool has_op = false;
	bool has_log = false;

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
			has_log = true;
		}
		else if (mr[4].matched)
		{
			node = new NameNode(mr[4]);
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
			else if (has_log)
			{
				LogNode* log_node = new LogNode();
				log_node->set_child(node);
				node = log_node;
				has_log = false;
			}

			last = node;
		}

		s = mr.suffix().str();
	}

	assert(node);
	return node;
}

Expression Expression::build_hack_expression(const std::string& val)
{
	Expression result;
	result.set_root(new LiteralNode(val));
	return result;
}