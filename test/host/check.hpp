/**
 * @file check.hpp
 * @brief Minimal assertion macros for the host tests (programs built with the host compiler, run on the development machine).
 *
 * A failed check prints `[FAIL] file:line: expression` and the test program keeps going; the `main` of the test
 * returns a failure code when `checksFailed` is not zero.
 */
#ifndef HOST_TEST_CHECK_HPP
#define HOST_TEST_CHECK_HPP

#include <stdio.h>

namespace host_test {

/** Number of checks evaluated so far. */
inline int checksRun = 0;
/** Number of checks that failed so far. */
inline int checksFailed = 0;

/**
 * @brief Counts one check and prints a line when it fails.
 * @param passed Result of the condition.
 * @param file Source file of the check (`__FILE__`).
 * @param line Source line of the check (`__LINE__`).
 * @param text Text of the checked expression.
 */
inline void record(bool passed, const char *file, int line, const char *text) {
	checksRun++;
	if (passed) return;
	checksFailed++;
	printf("[FAIL] %s:%d: %s\n", file, line, text);
}

} // namespace host_test

/** Checks that a condition is true. Evaluates it once; a failure is recorded and the test continues. */
#define CHECK_TRUE(condition) \
	host_test::record((condition), __FILE__, __LINE__, #condition)

/** Checks that a condition is false. */
#define CHECK_FALSE(condition) CHECK_TRUE(!(condition))

/** Checks that two values compare equal with `==`. Each argument is evaluated once; both must be comparable. */
#define CHECK_EQ(expected, actual) \
	host_test::record((expected) == (actual), __FILE__, __LINE__, \
					  #expected " == " #actual)

#endif // !HOST_TEST_CHECK_HPP
