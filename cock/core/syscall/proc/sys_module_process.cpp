#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::core::syscall {

using memory::VirtAddr;
using memory::VirtualMemoryManager;
using task::Process;
using task::Scheduler;
using task::TaskManager;
using task::Thread;
using utils::Logger;

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);

uintptr_t sys_module_process(uintptr_t name_ptr, uintptr_t length, uintptr_t,
							 uintptr_t, uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || length == 0 || length >= Process::MAX_NAME_LENGTH)
		return FAILURE;
	if (!current->getProcess()->getCapabilities().holdsGrantAuthority()) {
		Logger::trace("sys_module_process: PID %d holds no grant authority",
					  current->getProcess()->getId());
		return FAILURE;
	}
	if (!VirtualMemoryManager::isUserRangeValid(VirtAddr(name_ptr), length, false,
												current->getUserSpaceLimit()))
		return FAILURE;

	char name[Process::MAX_NAME_LENGTH] = {};
	memcpy(name, reinterpret_cast<const void *>(name_ptr), length);
	return TaskManager::findBootModuleId(name);
}

} // namespace cock::core::syscall
