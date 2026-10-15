#include <cock/core/hal/tasking.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using core::task::Scheduler;
using core::task::Thread;
using utils::Logger;

uintptr_t sys_request_resource(uintptr_t resource_id, uintptr_t size, uintptr_t,
							   uintptr_t, uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current) return (uintptr_t)-1;

	bool success = hal::grant_hardware_resource(current->getArchContext(),
												resource_id, size);
	if (!success) {
		Logger::warn("sys_request_resource: PID %d denied (id 0x%x, size %u)",
					 current->getId(), resource_id, size);
		return (uintptr_t)-1;
	}

	hal::apply_thread_context(current->getArchContext());
	return 0;
}

} // namespace cock::core::syscall
