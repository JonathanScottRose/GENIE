#include "pch.h"
#include "genie/log.h"

using namespace genie;
using namespace genie::log;

namespace
{
    void default_handler(const Message& msg)
    {
        const char* lvl_str = nullptr;

        switch (msg.level)
        {
        case Message::DEBUG: lvl_str = "Debug"; break;
        case Message::ERROR: lvl_str = "Error"; break;
        case Message::FATAL: lvl_str = "Fatal"; break;
        case Message::INFO: lvl_str = "Info"; break;
        case Message::WARN: lvl_str = "Warning"; break;
        default: assert(false);
        }

        printf("%s: %s\n", lvl_str, msg.msg.c_str());
    }

    Handler s_handler = default_handler;

    void msg_internal(Message::Level lvl, const char* fmt, va_list vl)
    {
        static char buf[4096];

        vsnprintf(buf, sizeof(buf), fmt, vl);
        Message msg;
        msg.level = lvl;
        msg.msg = std::string(buf);
        s_handler(msg);
    }
}

void log::set_handler(const Handler& h)
{
    s_handler = h;
}

void log::msg(Message::Level lvl, const char* fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    msg_internal(lvl, fmt, vl);
    va_end(vl);
}

#define templ(name,e) \
void log::name(const char* fmt, ...) \
{ \
    va_list vl; \
    va_start(vl, fmt); \
    msg_internal(Message::e, fmt, vl); \
    va_end(vl); \
}

templ(fatal,FATAL)
templ(error,ERROR)
templ(warn,WARN)
templ(info,INFO)
templ(debug,DEBUG)
