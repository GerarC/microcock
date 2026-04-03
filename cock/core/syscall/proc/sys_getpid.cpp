#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using core::task::Scheduler;
using core::task::Thread;

uintptr_t sys_getpid(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
					 uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (current) { return current->getId(); }

	return (uintptr_t)-1;
}

} // namespace cock::core::syscall
