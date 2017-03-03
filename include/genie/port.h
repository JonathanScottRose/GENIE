#pragma once

#include "genie/genie.h"
#include "genie/smart_enum.h"

namespace genie
{
    class Node;

    // Properties of an HDL Port
    struct HDLPortSpec
    {
        std::string name;
        std::string width;
        std::string depth;
    };

    // A binding to (part of) an HDL port
    struct HDLBindSpec
    {
        std::string lo_bit;
        std::string width;
        std::string lo_slice;
        std::string depth;
    };

    class Port : virtual public APIObject
    {
    public:
        SMART_ENUM(Dir, IN, OUT);

        virtual const std::string& get_name() const = 0;
        virtual Dir get_dir() const = 0;

    protected:
        virtual ~Port() = default;
    };

    class ConduitPort : virtual public Port
    {
    public:
        SMART_ENUM(Role, FWD, REV, IN, OUT, INOUT);

        virtual void add_signal(Role role, const std::string& tag, 
            const std::string& sig_name, const std::string& width = "1") = 0;
        virtual void add_signal(Role role, const std::string& tag, 
            const HDLPortSpec&, const HDLBindSpec&) = 0;
    };

    class RSPort : virtual public Port
    {
    public:
        SMART_ENUM(Role, VALID, READY, DATA, DATABUNDLE, EOP, ADDRESS);

        virtual void add_signal(Role role, const std::string& sig_name, 
            const std::string& width = "1") = 0;
        virtual void add_signal(Role role, const std::string& tag,
            const std::string& sig_name, const std::string& width = "1") = 0;
        virtual void add_signal(Role role, const HDLPortSpec&, const HDLBindSpec&) = 0;
        virtual void add_signal(Role role, const std::string& tag, 
            const HDLPortSpec&, const HDLBindSpec&) = 0;
    };
}