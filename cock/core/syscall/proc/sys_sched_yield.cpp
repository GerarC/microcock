#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <stdint.h>

namespace cock::core::syscall {

uintptr_t sys_sched_yield(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t){
	task::Scheduler::requestReschedule();
	return 0;
}

} // namespace cock::core::syscall
