#include <cock/core/syscall/proc/pending_image.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/pending_process.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using task::PendingProcesses;

uintptr_t sys_proc_discard(uintptr_t handle, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
						   uintptr_t) {
	if (pending_of_caller(handle) == nullptr) return PROC_FAILURE;
	PendingProcesses::discard(static_cast<uint32_t>(handle));
	return PROC_SUCCESS;
}

} // namespace cock::core::syscall
