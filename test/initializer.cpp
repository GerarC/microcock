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

namespace cock::test::task {
extern void test_process();
extern void test_scheduler();
extern void test_reaper();
} // namespace cock::test::task

namespace cock::test::arch {
extern void test_io_permissions();
extern void test_paging();
extern void test_pmm();
} // namespace cock::test::arch

namespace cock::test::security {
extern void test_capabilities();
} // namespace cock::test::security

namespace cock::test {
using utils::Logger;

void run_all_tests() {
	Logger::info("<=== INITIALIZING COCK TEST SUIT ===>");

	memory::test_heap();
	memory::test_new_operator();
	loader::test_elf_loader();
	task::test_process();
	arch::test_io_permissions();
	arch::test_pmm();
	arch::test_paging();
	security::test_capabilities();
	task::test_scheduler();
	task::test_reaper();

	Logger::info("<=== ENDING COCK TEST SUIT ===>\n");
	TestRunner::print_summary();

	core::hal::exit_emulator(TestRunner::tests_failed > 0 ? EXIT_CODE_FAILURE
														  : EXIT_CODE_SUCCESS);
}
} // namespace cock::test
