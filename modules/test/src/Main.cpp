#include "Test.hpp"

int main(void)
{
    int failedTests = 0;

    for (const auto& test : k3test::registry()) {
        const int before = k3test::failures();

        test.run();

        const bool ok = k3test::failures() == before;
        failedTests += ok ? 0 : 1;
        std::cout << (ok ? "[  OK  ] " : "[ FAIL ] ") << test.name << std::endl;
    }

    std::cout << k3test::registry().size() - failedTests << "/" << k3test::registry().size() << " tests passed" << std::endl;
    return failedTests == 0 ? 0 : 1;
}
