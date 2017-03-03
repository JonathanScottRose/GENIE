#pragma once

#include <string>
#include "genie/genie.h"
#include "genie/port.h"

namespace genie
{
    class ConduitPort;
    class RSPort;

    class Node : virtual public APIObject
    {
    public:
        virtual const std::string& get_name() const = 0;
		
		// Parameters
		virtual void set_int_param(const std::string& parm_name, int val) = 0;
		virtual void set_int_param(const std::string& parm_name, const std::string& expr) = 0;
		virtual void set_str_param(const std::string& parm_name, const std::string& str) = 0;
        virtual void set_lit_param(const std::string& parm_name, const std::string& str) = 0;

        // Ports
        virtual Port* create_clock_port(const std::string& name, Port::Dir dir, 
            const std::string& hdl_sig) = 0;
        virtual Port* create_reset_port(const std::string& name, Port::Dir dir, 
            const std::string& hdl_sig) = 0;
        virtual ConduitPort* create_conduit_port(const std::string& name, Port::Dir dir) = 0;
        virtual RSPort* create_rs_port(const std::string& name, Port::Dir dir, 
            const std::string& clk_port_name) = 0;

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