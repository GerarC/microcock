#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using core::task::Scheduler;
using core::task::Thread;
using utils::Logger;

uintptr_t sys_exit(uintptr_t status, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
				   uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (current) {
		Logger::trace("Syscall: EXIT (Thread %d, Code: %d)", current->getId(),
					  status);

		Scheduler::terminateCurrentThread();
	}

	return 0;
}

} // namespace cock::core::syscall
