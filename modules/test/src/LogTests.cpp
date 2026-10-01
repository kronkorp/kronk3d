#include "Test.hpp"
#include "io/ObjLoader.hpp"
#include "utils/Log.hpp"
#include <sstream>
#include <string>
#include <vector>

namespace
{

    struct Captured
    {
        k3::LogLevel level;
        std::string  message;
    };

    // Restores the console sink and the default level when the test ends.
    struct SinkGuard
    {
        ~SinkGuard()
        {
            k3::resetLogSink();
            k3::setLogLevel(k3::LogLevel::Info);
        }
    };

}

K3_TEST(log_sink_receives_library_messages)
{
    SinkGuard guard;
    std::vector<Captured> captured;
    k3::setLogSink([&](k3::LogLevel level, std::string_view message) { captured.push_back({level, std::string(message)}); });

    std::istringstream obj("v 0 0 0\nv 1 0 0\nv 0 1 0\nusemtl nowhere\nf 1 2 3\n");
    K3_REQUIRE(k3::ObjLoader::loadFromStream(obj, ""));

    K3_REQUIRE(captured.size() == 1);
    K3_CHECK(captured[0].level == k3::LogLevel::Warning);
    K3_CHECK(captured[0].message.find("nowhere") != std::string::npos);
}

K3_TEST(log_level_filters_and_braces_are_not_formatted)
{
    SinkGuard guard;
    std::vector<Captured> captured;
    k3::setLogSink([&](k3::LogLevel level, std::string_view message) { captured.push_back({level, std::string(message)}); });
    k3::setLogLevel(k3::LogLevel::Warning);

    k3::log(k3::LogLevel::Info, "dropped");
    k3::log(k3::LogLevel::Error, "kept {} {{}}", 42);
    k3::log(k3::LogLevel::Warning, std::string_view("a path with {braces}"));

    K3_REQUIRE(captured.size() == 2);
    K3_CHECK(captured[0].message == "kept 42 {}");
    K3_CHECK(captured[1].message == "a path with {braces}");
}
