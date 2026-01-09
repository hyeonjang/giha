#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <utility>

// ========== core registry =================================================
class TestRegistry {
public:
    using TestFunc = std::function<void()>;

    void add(std::string name, TestFunc fn) {
        tests_.emplace_back(std::move(name), std::move(fn));
    }

    int runAll() const {

        for (auto& [name, fn] : tests_) {
            try {
                fn();
                std::cout << "✔ " << name << '\n';
            } catch (const std::exception& e) {
                std::cerr << "✖ " << name << " — " << e.what() << '\n';
                return EXIT_FAILURE;
            }
        }
        std::cout << "All tests passed.\n";
        std::cout << "Total tests: " << tests_.size() << '\n' << '\n';

        return EXIT_SUCCESS;
    }
private:
    std::vector<std::pair<std::string, TestFunc>> tests_;
};

// ========== assertion =====================================================
#define REQUIRE(expr)                                                      \
    do {                                                                   \
        if (!(expr)) {                                                     \
            throw std::runtime_error(                                      \
                std::string("FAILED: ") + #expr +                          \
                "  (" __FILE__ ":" + std::to_string(__LINE__) + ")" );     \
        }                                                                  \
    } while (0)

// ========== registration plumbing ========================================
#define ST_CAT2(a,b) a##b
#define ST_CAT(a,b)  ST_CAT2(a,b)

struct TestRegistrar {
    TestRegistrar(TestRegistry& reg,
                  std::string name,
                  TestRegistry::TestFunc fn)
    { reg.add(std::move(name), std::move(fn)); }
};

/* Declare & define a test that goes into a given suite variable */
#define TEST_IN(suite, testname)                                           \
    void testname();                                                       \
    static TestRegistrar ST_CAT(_reg_, __COUNTER__)(suite, #testname, testname); \
    void testname()

#define TEST_IN_P(suite, testname, paramname, ...)                                    \
    template <typename ParamT>                                                       \
    void ST_CAT(testname, _impl)(ParamT paramname);                                   \
    namespace {                                                                       \
    struct ST_CAT(testname, _param_reg) {                                             \
        ST_CAT(testname, _param_reg)() {                                              \
            auto params = {__VA_ARGS__};                                              \
            std::size_t idx = 0;                                                      \
            for (auto param : params) {                                               \
                auto value = param;                                                   \
                std::string full_name =                                               \
                    std::string(#testname) + "[" + std::to_string(idx++) + "]";       \
                suite.add(full_name, [value]() {                                      \
                    ST_CAT(testname, _impl)(value);                                   \
                });                                                                   \
            }                                                                         \
        }                                                                             \
    };                                                                                \
    static ST_CAT(testname, _param_reg) ST_CAT(testname, _param_reg_instance);        \
    }                                                                                 \
    template <typename ParamT>                                                        \
    void ST_CAT(testname, _impl)(ParamT paramname)

// Convenience: create a suite variable in any source file
#define DECLARE_SUITE(suiteName)  TestRegistry suiteName{}

// Convenience: run a single suite (returns EXIT_SUCCESS / FAILURE code)
#define RUN_SUITE(suiteVar) printf("%s\n", #suiteVar); (suiteVar).runAll()

// You may still run ALL your suites manually from main if you want.
// Check that the [0, count) elements of lhs and rhs compare equal
#define REQUIRE_ARRAY_EQ(lhs, rhs, count)                                     \
    do {                                                                      \
        if (!std::equal((lhs), (lhs) + (count), (rhs))) {                     \
            throw std::runtime_error(                                         \
                std::string("FAILED: arrays differ  (" __FILE__ ":") +        \
                std::to_string(__LINE__) + ")");                              \
        }                                                                     \
    } while (0)

#define STRIFY(x) #x
#define STR(x) STRIFY(x)
#define WORKGROUP_SIZE 256
#define SUBGROUP_SIZE 32