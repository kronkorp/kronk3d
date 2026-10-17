#include "Log.hpp"
#include "Logger.hpp"
#include <mutex>

namespace
{

    void consoleSink(k3::LogLevel level, std::string_view message)
    {
        // "{}": the message must not be parsed as a format string.
        switch (level) {
            case k3::LogLevel::Debug:
                Logger::logger().debug("{}", message);
                break;
            case k3::LogLevel::Info:
                Logger::logger().info("{}", message);
                break;
            case k3::LogLevel::Warning:
                Logger::logger().warn("{}", message);
                break;
            case k3::LogLevel::Error:
                Logger::logger().error("{}", message);
                break;
        }
    }

    struct State
    {
        std::mutex   mutex;
        k3::LogSink  sink = consoleSink;
        k3::LogLevel minimum = k3::LogLevel::Info;
    };

    State& state()
    {
        static State instance;
        return instance;
    }

}

void k3::setLogSink(LogSink sink)
{
    std::lock_guard lock(state().mutex);
    state().sink = std::move(sink);
}

void k3::resetLogSink()
{
    setLogSink(consoleSink);
}

void k3::setLogLevel(LogLevel minimum)
{
    std::lock_guard lock(state().mutex);
    state().minimum = minimum;
}

void k3::log(LogLevel level, std::string_view message)
{
    State& s = state();
    std::lock_guard lock(s.mutex);

    if (s.sink && level >= s.minimum)
        s.sink(level, message);
}
