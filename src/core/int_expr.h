#pragma once

#include <string>

namespace genie
{
namespace impl
{ 
    class ParamResolver;

    namespace expr
    {
        class Node;
    }
    class IntExpr;

    class IntExpr
    {
    public:
        IntExpr();
        IntExpr(const IntExpr&);
        IntExpr(IntExpr&&);
        IntExpr(const std::string&);
        IntExpr(int);
        ~IntExpr();

        IntExpr& operator=(const IntExpr&);
        IntExpr& operator=(IntExpr&&);
        bool operator==(const IntExpr&) const;

        operator int() const;
        std::string to_string() const;

        bool is_const() const { return m_is_const; }
        int evaluate(ParamResolver&);

    protected:
        bool m_is_const;
        union
        {
            int m_const_val;
            expr::Node* m_expr_root;
        };
    };
}
}