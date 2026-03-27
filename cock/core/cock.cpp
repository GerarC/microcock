#include <cock/core/hal/utils.hpp>
#include <cock/core/cock.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/core/version.hpp>
#include <cock/utils/logger.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef COCK_TEST_MODE
#include <test/initializer.hpp>
#endif

namespace cock {

using core::task::Scheduler;
using core::task::Thread;
using core::task::ThreadPriority;
using core::task::ThreadResult;
using utils::Logger;
using utils::LogLevel;

ThreadResult kernel_idle() {
	while (true)
		core::hal::halt();
	return ThreadResult::success();
}

extern "C" void cock_main(void) {
	Logger::init(LogLevel::DEBUG);
	printf("Welcome to %s\n", core::VERSION_STRING);
	printf("Build: %s (%s)\n", COCK_BUILD_DATE, COCK_GIT_HASH);
	puts("Semillero de Linux UdeA");
	puts("SEIC UdeA");

	Scheduler::init();

#ifdef COCK_TEST_MODE
	Logger::warn("Setting Test mode.");
	test::run_all_tests();
	Logger::info("All test where executed.");
	__asm__ volatile("cli; hlt");
#endif
	Thread *idle_thread = new Thread(0, kernel_idle, ThreadPriority::IDLE);
	Scheduler::addThread(idle_thread);

	Scheduler::yield();

	Logger::fatal("Kernel panic: Returned to cock_main!");
	FOR_ETERNAL;
}

} // namespace cock
