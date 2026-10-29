#include <cock/abi/process.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using task::Process;
using task::TaskManager;
using task::Thread;

uintptr_t sys_proc_state(uintptr_t pid, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
						 uintptr_t) {
	if (pid > UINT32_MAX) return abi::process::NO_STATE_MASK;

	Thread *thread = TaskManager::getThreadByPID(static_cast<uint32_t>(pid));
	if (thread == nullptr || !thread->isMainThread()) return abi::process::NO_STATE_MASK;

	Process *process = thread->getProcess();
	if (process->isKernel()) return abi::process::NO_STATE_MASK;

	uint32_t mask = TaskManager::threadStateMask(*process);
	return mask == 0 ? abi::process::NO_STATE_MASK : mask;
}

} // namespace cock::core::syscall
