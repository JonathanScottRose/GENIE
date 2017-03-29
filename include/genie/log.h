#pragma once

#include <string>
#include <functional>

namespace genie
{
namespace log
{
    struct Message
    {
        enum Level
        {
            DEBUG,
            INFO,
            WARN,
            ERROR,
            FATAL
        };

        Level level;
        std::string msg;
    };

    void msg(Message::Level lvl, const char* fmt, ...);
    void debug(const char* fmt, ...);
    void info(const char* fmt, ...);
    void warn(const char* fmt, ...);
    void error(const char* fmt, ...);
    void fatal(const char* fmt, ...);

    using Handler = std::function<void(const Message&)>;
    void set_handler(const Handler&);
}
}