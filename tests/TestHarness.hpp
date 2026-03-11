#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace test {

struct Failure : public std::runtime_error {
    explicit Failure(const std::string& message)
        : std::runtime_error(message) {}
};

struct Case {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
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
            throw test::Failure(std::string("REQUIRE failed: ") + #cond); \
        } \
    } while (0)

#define REQUIRE_EQ(lhs, rhs) \
    do { \
        const auto& lhs_value = (lhs); \
        const auto& rhs_value = (rhs); \
        if (!(lhs_value == rhs_value)) { \
            throw test::Failure(std::string("REQUIRE_EQ failed: ") + #lhs + " == " + #rhs); \
        } \
    } while (0)
