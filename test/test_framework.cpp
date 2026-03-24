#include <test/test_framework.hpp>

namespace cock::test {

int TestRunner::tests_run = 0;
int TestRunner::tests_passed = 0;
int TestRunner::tests_failed = 0;

void TestRunner::begin_suite(const char *suite_name) {
	utils::Logger::info("=== RUNNING SUITE: %s ===", suite_name);
}

void TestRunner::end_suite() {
	utils::Logger::info("--------------------------------");
}

void TestRunner::print_summary() {
	utils::Logger::info("========= TEST SUMMARY =========");
	utils::Logger::info("Total asserts  : %d", tests_run);
	utils::Logger::info("Passed asserts : %d", tests_passed);

	if (tests_failed > 0) {
		utils::Logger::error("Failed asserts : %d", tests_failed);
	} else {
		utils::Logger::info("Failed : 0 (All tests passed!)");
	}
	utils::Logger::info("================================");
}

} // namespace cock::test
