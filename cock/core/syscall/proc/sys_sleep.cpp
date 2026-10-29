#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/core/task/timeouts.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using core::task::Scheduler;
using core::task::Thread;
using core::task::ThreadState;
using core::task::Timeouts;

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uintptr_t SUCCESS = 0;

uintptr_t sys_sleep(uintptr_t milliseconds, uintptr_t, uintptr_t, uintptr_t,
					uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || milliseconds > MAX_TIMEOUT_MILLISECONDS) return FAILURE;

	if (milliseconds == 0) {
		Scheduler::requestReschedule();
		return SUCCESS;
	}

	Timeouts::wait(*current, static_cast<uint32_t>(milliseconds),
				   ThreadState::BLOCKED);
	return SUCCESS;
}

} // namespace cock::core::syscall
