#include <cock/utils/logger.hpp>
#include <test/initializer.hpp>
#include <test/test_framework.hpp>

namespace cock::test::memory {
extern void test_heap();
extern void test_new_operator();
} // namespace cock::test::memory

namespace cock::test {
using utils::Logger;

void run_all_tests() {
	Logger::info("<=== INITIALIZING COCK TEST SUIT ===>");

	memory::test_heap();
	memory::test_new_operator();

	Logger::info("<=== ENDING COCK TEST SUIT ===>\n");
	TestRunner::print_summary();
}
} // namespace cock::test
