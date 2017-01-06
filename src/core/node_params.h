#pragma once

#include <string>
#include "int_expr.h"
#include "bits_val.h"

namespace genie
{
namespace impl
{
    class ParamResolver;
    class NodeParam;
    class NodeStringParam;
    class NodeIntParam;
    class NodeBitsParam;

    class ParamResolver
    {
    public:
        virtual NodeParam* resolve(const std::string&) = 0;
        virtual ~ParamResolver() = default;
    };

    class NodeParam
    {
    public:
        enum Type
        {
            STRING,
            INT,
            BITS
        };

        virtual ~NodeParam() = default;
        virtual Type get_type() const = 0;
        virtual NodeParam* clone() const = 0;
        virtual void resolve(ParamResolver&) = 0;
    };

    class NodeStringParam : public NodeParam
    {
    public:
        NodeStringParam() = default;
        NodeStringParam(const NodeStringParam&) = default;
        NodeStringParam(NodeStringParam&&) = default;
        NodeStringParam(const std::string&);
        NodeStringParam(std::string&&);
        ~NodeStringParam() = default;

        Type get_type() const override { return STRING; }
        NodeParam* clone() const override { return new NodeStringParam(*this); }
        void resolve(ParamResolver&) override;

        void set_val(const std::string& s) { m_str = s; }
        const std::string& get_val() const { return m_str; }

    protected:
        std::string m_str;
    };

    class NodeIntParam : public NodeParam
    {
    public:
        NodeIntParam() = default;
        NodeIntParam(const NodeIntParam&) = default;
        NodeIntParam(NodeIntParam&&) = default;
        NodeIntParam(int);
        NodeIntParam(const IntExpr&);
        NodeIntParam(IntExpr&&);
        NodeIntParam(const std::string&);
        ~NodeIntParam() = default;

        Type get_type() const override { return INT; }
        NodeParam* clone() const override { return new NodeIntParam(*this); }
        void resolve(ParamResolver&) override;

        IntExpr& get_val() { return m_int; }
        void set_val(const IntExpr&);
        void set_val(IntExpr&&);

    protected:
        IntExpr m_int;
    };

    class NodeBitsParam : public NodeParam
    {
    public:
        Type get_type() const override { return BITS; }
        NodeParam* clone() const override { return new NodeBitsParam(*this); }
        void resolve(ParamResolver&) override;

        BitsVal& get_val() { return m_val; }
        void set_val(const BitsVal&);
        void set_val(BitsVal&&);

    protected:
        BitsVal m_val;
    };
}
}