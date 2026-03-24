#ifndef COCK_TEST_FRAMEWORK_HPP
#define COCK_TEST_FRAMEWORK_HPP

#include <cock/utils/logger.hpp>

namespace cock::test {

class TestRunner {
public:
    static int tests_run;
    static int tests_passed;
    static int tests_failed;

    static void begin_suite(const char* suite_name);
    static void end_suite();
    static void print_summary();
};

} // namespace cock::test

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

#define EXPECT_FALSE(cond) EXPECT_TRUE(!(cond))

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

#define EXPECT_NOT_NULL(ptr) EXPECT_NEQ(nullptr, ptr)
#define EXPECT_NULL(ptr)     EXPECT_EQ(nullptr, ptr)

#endif // !COCK_TEST_FRAMEWORK_HPP
