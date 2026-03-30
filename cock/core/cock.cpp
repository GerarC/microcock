#include <cock/core/cock.hpp>
#include <cock/core/hal/utils.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/core/version.hpp>
#include <cock/utils/logger.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef COCK_TEST_MODE
#include <test/initializer.hpp>
#endif

namespace cock {

using core::task::Scheduler;
using core::task::TaskManager;
using core::task::Thread;
using core::task::ThreadPriority;
using core::task::ThreadResult;
using core::task::ThreadType;
using utils::Logger;
using utils::LogLevel;

ThreadResult kernel_idle() {
	while (true) {
		TaskManager::buryDeadThreads();
		core::hal::halt();
	}
	return ThreadResult::success();
}

void user_process_example() {
	const char *message = "Hello from cock ring 3!!!\n";
	size_t msg_len = strlen(message);

	unsigned char shellcode[] = {0xE4, 0x60, 0xB8, 0x04, 0x00, 0x00, 0x00, 0xBB, 0x01, 0x00, 0x00,
								 0x00, 0xB9, 0x24, 0x00, 0x00, 0x40, 0xBA, (unsigned char)msg_len,
								 0x00, 0x00, 0x00, 0xCD, 0x80, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xBB,
								 0x00, 0x00, 0x00, 0x00, 0xCD, 0x80};
	Thread *user_thread =
		new Thread(shellcode, sizeof(shellcode), ThreadPriority::NORMAL, true);

	uintptr_t old_cr3 = core::hal::get_current_address_space();
	core::hal::switch_address_space(user_thread->getAddressSpace());

	memcpy(reinterpret_cast<void *>(0x40000024), message, msg_len);

	core::hal::switch_address_space(old_cr3);

	Scheduler::addThread(user_thread);
}

extern "C" void cock_main(void) {
	Logger::init(LogLevel::DEBUG);
	printf("Welcome to %s\n", core::VERSION_STRING);
	printf("Build: %s (%s)\n", COCK_BUILD_DATE, COCK_GIT_HASH);
	puts("Semillero de Linux UdeA");
	puts("SEIC UdeA");

#ifdef COCK_TEST_MODE
	Logger::warn("Setting Test mode.");
	test::run_all_tests();
	Logger::info("All test where executed.");
	FOR_ETERNAL;
#endif

	TaskManager::init();
	Scheduler::init();

	Thread *idle_thread = new Thread(kernel_idle, ThreadPriority::IDLE);
	Scheduler::addThread(idle_thread);

	user_process_example();

	core::hal::manual_timer();
	Logger::fatal("Kernel panic: Returned to cock_main!");
	FOR_ETERNAL;
}

} // namespace cock
