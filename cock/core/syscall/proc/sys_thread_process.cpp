#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using task::TaskManager;
using task::Thread;
using task::ThreadState;

namespace {

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);

}

uintptr_t sys_thread_process(uintptr_t tid, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
							 uintptr_t) {
	if (tid > UINT32_MAX) return FAILURE;

	Thread *thread = TaskManager::getThreadByPID(static_cast<uint32_t>(tid));
	if (thread == nullptr || thread->getState() == ThreadState::DEAD ||
		thread->getProcess()->isKernel())
		return FAILURE;
	return thread->getProcess()->getId();
}

} // namespace cock::core::syscall
