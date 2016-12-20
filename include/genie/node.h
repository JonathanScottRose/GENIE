#pragma once

#include <string>

namespace genie
{
    class Node
    {
    public:
        virtual const std::string& get_name() const = 0;

    protected:
        ~Node() = default;
    };
}