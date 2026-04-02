#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using core::task::Scheduler;

uintptr_t sys_sched_yield(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t){
	return 0;
}

} // namespace cock::core::syscall

