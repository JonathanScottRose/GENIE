#pragma once

#include <string>

namespace genie
{
namespace impl
{
    class ParamResolver;

namespace expr
{
    class Node
    {
    public:
        virtual ~Node() = default;

        virtual int evaluate(ParamResolver& r) const = 0;
        virtual Node* clone() const = 0;
        virtual std::string to_string() const = 0;
    };

    class OpNode : public Node
    {
    public:
        OpNode(char op) : m_op(op) {}
        ~OpNode() = default;

        char get_op() const { return m_op; }

    protected:
        char m_op;
    };

    class BinOpNode : public OpNode
    {
    public:
        BinOpNode(char op);
        ~BinOpNode();

        void set_lhs(Node* node);
        void set_rhs(Node* node);
        Node* get_lhs() const { return m_lhs; }
        Node* get_rhs() const { return m_rhs; }

        int evaluate(ParamResolver& r) const override;
        Node* clone() const override;
        std::string to_string() const override;

    protected:
        void check_lhs_rhs() const;

        Node* m_lhs;
        Node* m_rhs;
    };

    class UnOpNode : public OpNode
    {
    public:
        UnOpNode(char op);
        ~UnOpNode();

        void set_rhs(Node* node);
        Node* get_rhs() const { return m_rhs; }

        int evaluate(ParamResolver& r) const override;
        Node* clone() const override;
        std::string to_string() const override;

    protected:
        Node* m_rhs;
    };

    class ConstNode : public Node
    {
    public:
        ConstNode();
        ConstNode(int val);
        ~ConstNode() = default;

        void set_value(int val);

        Node* clone() const override;
        int evaluate(ParamResolver& r) const override;
        std::string to_string() const override;

    protected:
        int m_value;
    };

    class NameNode : public Node
    {
    public:
        NameNode();
        NameNode(const std::string& expr_name);
        ~NameNode() = default;

        void set_name(const std::string&);
        const std::string& get_name() const { return m_ref; }

        Node* clone() const override;
        int evaluate(ParamResolver& r) const override;
        std::string to_string() const override;

    protected:
        std::string m_ref;
    };

    Node* parse(const std::string&);
}
}
}