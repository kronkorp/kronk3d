/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Where kronk3d's messages go
*/
#pragma once

#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace k3
{

    enum class LogLevel {
        Debug,
        Info,
        Warning,
        Error
    };

    // Receives every message kronk3d emits (calls are serialized, from whichever thread logs).
    using LogSink = std::function<void(LogLevel level, std::string_view message)>;

    // Redirects kronk3d's messages, e.g. to the application's own logger. The default sink prints
    // them on the console; an empty sink silences kronk3d entirely.
    void setLogSink(LogSink sink);

    // Back to the default console sink.
    void resetLogSink();

    // Messages below this level are dropped (default: Info).
    void setLogLevel(LogLevel minimum);

    void log(LogLevel level, std::string_view message);

    template<typename... Args>
    void log(LogLevel level, std::format_string<Args...> format, Args&&... args)
    {
        log(level, std::string_view(std::format(format, std::forward<Args>(args)...)));
    }

}
