#pragma once

#include <string>
#include "genie/genie.h"
#include "genie/port.h"
#include "genie/link.h"

namespace genie
{
    class PortConduit;
    class PortRS;

    class Node : virtual public HierObject
    {
    public:
        virtual const std::string& get_name() const = 0;
		
		// Parameters
		virtual void set_int_param(const std::string& parm_name, int val) = 0;
		virtual void set_int_param(const std::string& parm_name, const std::string& expr) = 0;
		virtual void set_str_param(const std::string& parm_name, const std::string& str) = 0;
        virtual void set_lit_param(const std::string& parm_name, const std::string& str) = 0;

        // Ports
        virtual Port* create_clock_port(const std::string& name, genie::Port::Dir dir, 
            const std::string& hdl_sig) = 0;
        virtual Port* create_reset_port(const std::string& name, genie::Port::Dir dir,
            const std::string& hdl_sig) = 0;
        virtual PortConduit* create_conduit_port(const std::string& name, genie::Port::Dir dir) = 0;
        virtual PortRS* create_rs_port(const std::string& name, genie::Port::Dir dir,
            const std::string& clk_port_name) = 0;

    protected:
        ~Node() = default;
    };

    class Module : virtual public Node
    {
    public:
		virtual Link* create_internal_link(PortRS* src, PortRS* sink, unsigned latency) = 0;

    protected:
        ~Module() = default;
    };

    class System : virtual public Node
    {
    public:
        virtual void create_sys_param(const std::string&) = 0;

		virtual Link* create_clock_link(HierObject* src, HierObject* sink) = 0;
		virtual Link* create_reset_link(HierObject* src, HierObject* sink) = 0;
		virtual Link* create_conduit_link(HierObject* src, HierObject* sink) = 0;
		virtual LinkRS* create_rs_link(HierObject* src, HierObject* sink) = 0;
		virtual Link* create_topo_link(HierObject* src, HierObject* sink) = 0;

    protected:
        ~System() = default;
    };
}