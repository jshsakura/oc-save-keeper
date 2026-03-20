#include "tests/TestHarness.hpp"
#include <cstdio>

int main() {
    for (const auto& testCase : test::registry()) {
        std::printf("[RUN] %s\n", testCase.name.c_str());
        testCase.fn();
        std::printf("[PASS] %s\n", testCase.name.c_str());
    }

    std::printf("%zu test(s) passed\n", test::registry().size());
    return 0;
}
