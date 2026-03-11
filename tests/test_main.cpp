#include "tests/TestHarness.hpp"

#include <cstdio>

int main() {
    int failures = 0;

    for (const auto& testCase : test::registry()) {
        try {
            testCase.fn();
            std::printf("[PASS] %s\n", testCase.name.c_str());
        } catch (const std::exception& ex) {
            std::printf("[FAIL] %s: %s\n", testCase.name.c_str(), ex.what());
            ++failures;
        }
    }

    if (failures != 0) {
        std::printf("%d test(s) failed\n", failures);
        return 1;
    }

    std::printf("%zu test(s) passed\n", test::registry().size());
    return 0;
}
