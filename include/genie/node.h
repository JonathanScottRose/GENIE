#pragma once

#include <string>
#include "genie/genie.h"

namespace genie
{
    class Node : virtual public APIObject
    {
    public:
        virtual const std::string& get_name() const = 0;
		
		// Parameters
		virtual void set_int_param(const std::string& parm_name, int val) = 0;
		virtual void set_int_param(const std::string& parm_name, const std::string& expr) = 0;
		virtual void set_str_param(const std::string& parm_name, const std::string& str) = 0;
        virtual void set_lit_param(const std::string& parm_name, const std::string& str) = 0;

    protected:
        ~Node() = default;
    };

    class Module : virtual public Node
    {
    public:

    protected:
        ~Module() = default;
    };

    class System : virtual public Node
    {
    public:
        virtual void create_sys_param(const std::string&) = 0;

    protected:
        ~System() = default;
    };
}