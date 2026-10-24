#include <cock/utils/logger.hpp>
#include <hal/cpu.hpp>
#include <test/initializer.hpp>
#include <test/test_framework.hpp>

namespace cock::test::memory {
extern void test_heap();
extern void test_new_operator();
extern void test_shared_memory();
} // namespace cock::test::memory

namespace cock::test::loader {
extern void test_elf_loader();
} // namespace cock::test::loader

namespace cock::test::task {
extern void test_process();
extern void test_scheduler();
extern void test_reaper();
extern void test_timeouts();
extern void test_threads();
extern void test_sync_ipc();
extern void test_exit_notice();
} // namespace cock::test::task

namespace cock::test::arch {
extern void test_io_permissions();
extern void test_paging();
extern void test_pmm();
extern void test_frames();
} // namespace cock::test::arch

namespace cock::test::security {
extern void test_capabilities();
extern void test_revocation();
extern void test_capability_transfer();
extern void test_ipc_access();
} // namespace cock::test::security

namespace cock::test {
using utils::Logger;

void run_all_tests() {
	Logger::info("<=== INITIALIZING COCK TEST SUIT ===>");

	memory::test_heap();
	memory::test_new_operator();
	memory::test_shared_memory();
	loader::test_elf_loader();
	task::test_process();
	arch::test_io_permissions();
	arch::test_pmm();
	arch::test_frames();
	arch::test_paging();
	security::test_capabilities();
	security::test_revocation();
	security::test_capability_transfer();
	security::test_ipc_access();
	task::test_scheduler();
	task::test_reaper();
	task::test_timeouts();
	task::test_threads();
	task::test_sync_ipc();
	task::test_exit_notice();

	Logger::info("<=== ENDING COCK TEST SUIT ===>\n");
	TestRunner::print_summary();

	core::hal::exit_emulator(TestRunner::tests_failed > 0 ? EXIT_CODE_FAILURE
														  : EXIT_CODE_SUCCESS);
}
} // namespace cock::test
