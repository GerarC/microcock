/**
 * @file test_framework.hpp
 * @brief Assertion macros and counters for the tests that run inside the kernel.
 *
 * Tests group their checks in suites (`begin_suite`, `end_suite`) and use the `EXPECT_*` macros. A failed check
 * logs `[FAIL] file:line: ...` through the kernel logger and the run continues; `print_summary` totals the counters.
 */
#ifndef COCK_TEST_FRAMEWORK_HPP
#define COCK_TEST_FRAMEWORK_HPP

#include <cock/utils/logger.hpp>
#include <stdint.h>

namespace cock::test {

/** Value written to the emulator exit device when every test passed (the emulator exits with this code shifted, see the QEMU scripts). */
constexpr uint8_t EXIT_CODE_SUCCESS = 0x10;
/** Value written to the emulator exit device when at least one test failed. */
constexpr uint8_t EXIT_CODE_FAILURE = 0x11;

/** Global counters and suite markers of the in-kernel tests. Single threaded. */
class TestRunner {
public:
    /** Checks evaluated. */
    static int tests_run;
    /** Checks that passed. */
    static int tests_passed;
    /** Checks that failed. */
    static int tests_failed;

    /**
     * @brief Marks the start of a suite and logs its name.
     * @param suite_name Null terminated name shown in the log.
     */
    static void begin_suite(const char* suite_name);
    /** @brief Marks the end of the current suite and logs its totals. */
    static void end_suite();
    /** @brief Logs the totals of the whole run: checks run, passed and failed. */
    static void print_summary();
};

} // namespace cock::test

/** Checks that a condition is true. Evaluates it once; counts it and logs a failure without stopping. */
#define EXPECT_TRUE(cond) \
    do { \
        cock::test::TestRunner::tests_run++; \
        if ((cond)) { \
            cock::test::TestRunner::tests_passed++; \
        } else { \
            cock::test::TestRunner::tests_failed++; \
            cock::utils::Logger::error("[FAIL] %s:%d: Expected true: %s", __FILE__, __LINE__, #cond); \
        } \
    } while(0)

/** Checks that a condition is false. */
#define EXPECT_FALSE(cond) EXPECT_TRUE(!(cond))

/** Checks that two values are equal with `==`. Each is evaluated once and stored in a local of deduced type, so both must be copyable. Opens its own scope, so it can be used several times in one block. */
#define EXPECT_EQ(expected, actual) \
    do { \
        cock::test::TestRunner::tests_run++; \
        auto e = (expected); \
        auto a = (actual); \
        if (e == a) { \
            cock::test::TestRunner::tests_passed++; \
        } else { \
            cock::test::TestRunner::tests_failed++; \
            cock::utils::Logger::error("[FAIL] %s:%d: Expected %s == %s", __FILE__, __LINE__, #expected, #actual); \
        } \
    } while(0)

/** Checks that two values differ with `!=`. Same evaluation rules as `EXPECT_EQ`. */
#define EXPECT_NEQ(expected, actual) \
    do { \
        cock::test::TestRunner::tests_run++; \
        auto e = (expected); \
        auto a = (actual); \
        if (e != a) { \
            cock::test::TestRunner::tests_passed++; \
        } else { \
            cock::test::TestRunner::tests_failed++; \
            cock::utils::Logger::error("[FAIL] %s:%d: Expected %s != %s", __FILE__, __LINE__, #expected, #actual); \
        } \
    } while(0)

/** Checks that a pointer is not null. */
#define EXPECT_NOT_NULL(ptr) EXPECT_NEQ(nullptr, ptr)
/** Checks that a pointer is null. */
#define EXPECT_NULL(ptr)     EXPECT_EQ(nullptr, ptr)

#endif // !COCK_TEST_FRAMEWORK_HPP
