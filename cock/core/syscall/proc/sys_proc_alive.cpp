#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using task::TaskManager;
using task::Thread;
using task::ThreadState;

namespace {

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uintptr_t SUCCESS = 0;

}

uintptr_t sys_proc_alive(uintptr_t pid, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
						 uintptr_t) {
	Thread *thread = TaskManager::getThreadByPID(static_cast<uint32_t>(pid));
	return thread != nullptr && thread->getState() != ThreadState::DEAD ? SUCCESS
																		: FAILURE;
}

} // namespace cock::core::syscall
