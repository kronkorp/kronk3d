/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Minimal self-registering test harness (no external dependency)
*/
#pragma once

#include <cmath>
#include <iostream>
#include <string_view>
#include <vector>

namespace k3test
{

    struct TestCase
    {
        std::string_view name;
        void (*run)();
    };

    inline std::vector<TestCase>& registry()
    {
        static std::vector<TestCase> tests;
        return tests;
    }

    inline int& failures()
    {
        static int count = 0;
        return count;
    }

    inline bool add(std::string_view name, void (*run)())
    {
        registry().push_back({name, run});
        return true;
    }

    inline void fail(const char* file, int line, std::string_view expression)
    {
        ++failures();
        std::cerr << "    " << file << ":" << line << ": check failed: " << expression << "\n";
    }

}

#define K3_TEST(name)                                                                   \
    static void name();                                                                 \
    [[maybe_unused]] static const bool name##_registered = k3test::add(#name, name);    \
    static void name()

#define K3_CHECK(expr) \
    do { if (!(expr)) k3test::fail(__FILE__, __LINE__, #expr); } while (0)

#define K3_CHECK_NEAR(a, b, eps) \
    do { if (!(std::abs((a) - (b)) <= (eps))) k3test::fail(__FILE__, __LINE__, #a " ~= " #b); } while (0)

// Stops the current test on failure (for preconditions the rest of the test depends on).
#define K3_REQUIRE(expr) \
    do { if (!(expr)) { k3test::fail(__FILE__, __LINE__, #expr); return; } } while (0)
