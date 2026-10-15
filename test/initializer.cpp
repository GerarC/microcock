#include <cock/utils/logger.hpp>
#include <hal/cpu.hpp>
#include <test/initializer.hpp>
#include <test/test_framework.hpp>

namespace cock::test::memory {
extern void test_heap();
extern void test_new_operator();
} // namespace cock::test::memory

namespace cock::test::loader {
extern void test_elf_loader();
} // namespace cock::test::loader

namespace cock::test {
using utils::Logger;

void run_all_tests() {
	Logger::info("<=== INITIALIZING COCK TEST SUIT ===>");

	memory::test_heap();
	memory::test_new_operator();
	loader::test_elf_loader();

	Logger::info("<=== ENDING COCK TEST SUIT ===>\n");
	TestRunner::print_summary();

	core::hal::exit_emulator(TestRunner::tests_failed > 0 ? EXIT_CODE_FAILURE
														  : EXIT_CODE_SUCCESS);
}
} // namespace cock::test
