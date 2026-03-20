#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

namespace test {

struct Case {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline void report_failure(const std::string& message) {
    std::cerr << "\n[FAIL] " << message << std::endl;
    std::exit(1);
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back(Case{name, std::move(fn)});
    }
};

} // namespace test

#define TEST_CONCAT_IMPL(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_IMPL(a, b)

#define TEST_CASE(name) \
    static void TEST_CONCAT(test_case_fn_, __LINE__)(); \
    static test::Registrar TEST_CONCAT(test_case_registrar_, __LINE__)(name, TEST_CONCAT(test_case_fn_, __LINE__)); \
    static void TEST_CONCAT(test_case_fn_, __LINE__)()

#define REQUIRE(cond) \
    do { \
        if (!(cond)) { \
            test::report_failure(std::string("REQUIRE failed: ") + #cond); \
        } \
    } while (0)

#define REQUIRE_EQ(lhs, rhs) \
    do { \
        const auto& lhs_value = (lhs); \
        const auto& rhs_value = (rhs); \
        if (!(lhs_value == rhs_value)) { \
            test::report_failure(std::string("REQUIRE_EQ failed: ") + #lhs + " == " + #rhs); \
        } \
    } while (0)
