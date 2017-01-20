#include "pch.h"
#include "genie/genie.h"
#include "int_expr_nodes.h"
#include "int_expr.h"
#include "params.h"
#include "util.h"

using namespace genie::impl;
using namespace genie::impl::expr;

//
// BinOpNode
//

BinOpNode::BinOpNode(char op)
    : OpNode(op), m_lhs(nullptr), m_rhs(nullptr)
{
}

BinOpNode::~BinOpNode()
{
    delete m_lhs;
    delete m_rhs;
}

void BinOpNode::set_lhs(Node * node)
{
    if (m_lhs) delete m_lhs;
    m_lhs = node;
}

void BinOpNode::set_rhs(Node * node)
{
    if (m_rhs) delete m_rhs;
    m_rhs = node;
}

int BinOpNode::evaluate(ParamResolver& r) const
{
    check_lhs_rhs();

    int lhs = m_lhs->evaluate(r);
    int rhs = m_rhs->evaluate(r);

    switch(m_op)
    {
    case '+': return lhs + rhs; break;
    case '-': return lhs - rhs; break;
    case '*': return lhs * rhs; break;
    case '/': return lhs / rhs; break;
    default:
        throw Exception("BinOpNode has undefined op " + m_op); break;
    }
}

Node * BinOpNode::clone() const
{
    BinOpNode* result = new BinOpNode(m_op);
    if (m_lhs) result->set_lhs(m_lhs->clone());
    if (m_rhs) result->set_rhs(m_rhs->clone());

    return result;
}

std::string BinOpNode::to_string() const
{
    check_lhs_rhs();

    std::string lhs = m_lhs->to_string();
    std::string rhs = m_rhs->to_string();
    return lhs + m_op + rhs;
}

void BinOpNode::check_lhs_rhs() const
{
    assert(m_lhs);
    assert(m_rhs);
}

//
// UnOpNode
//

UnOpNode::UnOpNode(char op)
    :OpNode(op), m_rhs(nullptr)
{
}

UnOpNode::~UnOpNode()
{
    delete m_rhs;
}

void UnOpNode::set_rhs(Node * node)
{
    delete m_rhs;
    m_rhs = node;
}

int UnOpNode::evaluate(ParamResolver& r) const
{
    assert(m_rhs);

    switch (m_op)
    {
    case '%': return util::log2(m_rhs->evaluate(r));
    case '(': return m_rhs->evaluate(r);
    default:
        throw Exception("UnOpNode has undefined operation " + m_op);
    }
}

Node * UnOpNode::clone() const
{
    UnOpNode* result = new UnOpNode(m_op);
    if (m_rhs) result->set_rhs(m_rhs->clone());
    return result;
}

std::string UnOpNode::to_string() const
{
    assert(m_rhs);
    switch (m_op)
    {
    case '%': return "%" + m_rhs->to_string();
    case '(': return "(" + m_rhs->to_string() + ")";
    default:
        throw Exception("UnOpNode has undefined operation");
    }
}

//
// ConstNode
//

ConstNode::ConstNode()
    :ConstNode(0)
{
}

ConstNode::ConstNode(int val)
    :m_value(val)
{
}

void ConstNode::set_value(int val)
{
    m_value = val;
}

Node* ConstNode::clone() const
{
    return new ConstNode(m_value);
}

int ConstNode::evaluate(ParamResolver& r) const
{
    return m_value;
}

std::string ConstNode::to_string() const
{
    return std::to_string(m_value);
}

//
// ConstNode
//

NameNode::NameNode()
    :NameNode("<undefined ref>")
{
}

NameNode::NameNode(const std::string & expr_name)
    :m_ref(expr_name)
{
}

void NameNode::set_name(const std::string & name)
{
    m_ref = name;
}

Node * NameNode::clone() const
{
    return new NameNode(m_ref);
}

int NameNode::evaluate(ParamResolver& r) const
{
    NodeParam* resolved_param = r.resolve(m_ref);

    if (resolved_param->get_type() != NodeParam::INT)
        throw Exception(m_ref + " not a const-evaluatable integer expression");

    auto parm_int = util::as_a<NodeIntParam>(resolved_param);
    assert(parm_int);

    return parm_int->get_val().evaluate(r);
}

std::string NameNode::to_string() const
{
    return m_ref;
}

//
// Parse expression function
//

namespace
{
    struct ParseState
    {
        std::vector<OpNode*> op_stack;
        std::vector<Node*> val_stack;
    };

    bool should_eval(char op1, char op2)
    {
        const char* ops = nullptr;
        
        switch (op1)
        {
        case '+':
        case '-':
            ops = "+-)";
            break;
        case '*':
        case '/':
        case '%':
            ops = "+-*/)";
            break;
        case '(':
            ops = ")";
            break;
        default:
            assert(false);
        }

        return std::strchr(ops, op2) != nullptr;
    }

    bool apply_op(ParseState& state)
    {
        // Assumes the operator stack is not empty
        OpNode* op = state.op_stack.back();
        state.op_stack.pop_back();

        // Determine whether unary or binary operator, make sure it's exactly one or the other
        UnOpNode* un_op = util::as_a<UnOpNode>(op);
        BinOpNode* bin_op = util::as_a<BinOpNode>(op);
        assert( (un_op != nullptr) != (bin_op != nullptr) );

        if (un_op)
        {
            // Check that the right number of arguments exists
            if (state.val_stack.size() < 1)
                return false;

            Node* val = state.val_stack.back();
            state.val_stack.pop_back();

            // Assign argument to operator, place onto value stack
            un_op->set_rhs(val);
        }
        else if (bin_op)
        {
            if (state.val_stack.size() < 2)
                return false;

            Node* rhs = state.val_stack.back();
            state.val_stack.pop_back();
            Node* lhs = state.val_stack.back();
            state.val_stack.pop_back();

            bin_op->set_lhs(lhs);
            bin_op->set_rhs(rhs);
        }

        // Push the operation (now with arguments) onto the value stack
        state.val_stack.push_back(op);
        return true;
    }
}

Node * expr::parse(const std::string& str)
{
    static std::regex regex(R"(\s*(\d+)|([+-/*%\)\(])|([[:alnum:]_]+))");

    ParseState state;   
    auto str_begin = str.begin();

    while (str_begin != str.end())
    {
        std::smatch mr;

        // Get next token
        bool valid_tok = std::regex_search(str_begin, str.end(), mr, regex);
        if (!valid_tok)
        {
            throw Exception("Invalid token " + std::string(str_begin, str.end()) +
                " in expression " + str);
        }

        // Check the token type
        if (mr[1].matched)
        {
            // Integer constant - push to value stack
            ConstNode* node = new ConstNode(std::stoi(mr[1]));
            state.val_stack.push_back(node);
        }
        else if (mr[2].matched)
        {
            // Operator
            OpNode* new_node = nullptr;
            char op = mr[2].str().at(0);

            switch(op)
            {
            case '%':
            case '(':
                new_node = new UnOpNode(op);
                break;
            case '+':
            case '-':
            case '/':
            case '*':
                new_node = new BinOpNode(op);
                break;
            case ')':
                // Closing bracket handled automatically below,
                // but no opnode gets created.
                break;
            }

            // Before pushing new operator, evaluate all previous higher-precedence operators
            while (!state.op_stack.empty())
            {
                char old_op = state.op_stack.back()->get_op();
                
                if (should_eval(old_op, op))
                {
                    if (!apply_op(state))
                        throw Exception("Bad expression: " + str);

                    // Terminate when matching paren found and the node for '(' has been
                    // appplied
                    if (op == ')' && old_op == '(')
                        break;
                }
                else
                {
                    break;
                }
            }

            // Push the parsed op onto opstack. Parens don't get nodes created.
            if (op != ')')
                state.op_stack.push_back(new_node);
        }
        else if (mr[3].matched)
        {
            // Alphanumeric string, symbolic reference to a parameter
            auto node = new NameNode(util::str_toupper(mr[3]));
            state.val_stack.push_back(node);
        }
        else
        {
            // Should have caught this if valid_tok is false
            assert(false);
        }

        // Advance parse position to one past the end of the match
        str_begin = mr.suffix().first;
    }

    // Apply any remaining operators
    while (!state.op_stack.empty())
    {
        if (!apply_op(state))
            throw Exception("Bad expression: " + str);
    }

    // Should be left with one value on the value stack
    if (state.val_stack.size() != 1)
        throw Exception("Bad expression: " + str);
    
    return state.val_stack.back();
}
