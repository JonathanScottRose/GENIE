#pragma once

#include <string>

namespace genie
{
    class Node
    {
    public:
        virtual const std::string& get_name() const = 0;
		
		// Parameters
		virtual void set_int_param(const std::string& parm_name, int val) = 0;
		virtual void set_int_param(const std::string& parm_name, const std::string& expr) = 0;
		virtual void set_str_param(const std::string& parm_name, const std::string& str) = 0;

    protected:
        ~Node() = default;
    };
}