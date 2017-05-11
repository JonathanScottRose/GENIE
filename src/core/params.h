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
            BITS,
            SYS,
            LITERAL
        };

        virtual ~NodeParam() = default;
        virtual Type get_type() const = 0;
        virtual NodeParam* clone() const = 0;
        virtual void resolve(ParamResolver&) = 0;
		virtual bool is_resolved() const = 0;
    };

    class NodeSysParam : public NodeParam
    {
    public:
        Type get_type() const override { return SYS; }
        NodeParam* clone() const override { return new NodeSysParam(*this); }
        void resolve(ParamResolver&) override {}
		bool is_resolved() const override { return true; }
		// no data members: parameter exists as placeholder without a value
    };

    class NodeLiteralParam : public NodeParam
    {
    public:
        NodeLiteralParam() = default;
        NodeLiteralParam(const NodeLiteralParam&) = default;
        NodeLiteralParam(NodeLiteralParam&&) = default;
        NodeLiteralParam(const std::string&);
        NodeLiteralParam(std::string&&);
        ~NodeLiteralParam() = default;

        Type get_type() const override { return LITERAL; }
        NodeParam* clone() const override { return new NodeLiteralParam(*this); }
        void resolve(ParamResolver&) override {};
		bool is_resolved() const override { return true; }

        void set_val(const std::string& s) { m_str = s; }
        const std::string& get_val() const { return m_str; }

    protected:
        std::string m_str;
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
        void resolve(ParamResolver&) override {};
		bool is_resolved() const override { return true; }

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
		bool is_resolved() const override;

        IntExpr& get_val() { return m_int; }
        void set_val(const IntExpr&);
        void set_val(IntExpr&&);

    protected:
        IntExpr m_int;
    };

    class NodeBitsParam : public NodeParam
    {
    public:
		NodeBitsParam() = default;
		NodeBitsParam(const BitsVal&);

        Type get_type() const override { return BITS; }
        NodeParam* clone() const override { return new NodeBitsParam(*this); }
        void resolve(ParamResolver&) override;
		bool is_resolved() const override { return true; }

        BitsVal& get_val() { return m_val; }
        void set_val(const BitsVal&);
        void set_val(BitsVal&&);

    protected:
        BitsVal m_val;
    };
}
}