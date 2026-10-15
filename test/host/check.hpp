#ifndef HOST_TEST_CHECK_HPP
#define HOST_TEST_CHECK_HPP

#include <stdio.h>

namespace host_test {

inline int checksRun = 0;
inline int checksFailed = 0;

inline void record(bool passed, const char *file, int line, const char *text) {
	checksRun++;
	if (passed) return;
	checksFailed++;
	printf("[FAIL] %s:%d: %s\n", file, line, text);
}

} // namespace host_test

#define CHECK_TRUE(condition) \
	host_test::record((condition), __FILE__, __LINE__, #condition)

#define CHECK_FALSE(condition) CHECK_TRUE(!(condition))

#define CHECK_EQ(expected, actual) \
	host_test::record((expected) == (actual), __FILE__, __LINE__, \
					  #expected " == " #actual)

#endif // !HOST_TEST_CHECK_HPP
