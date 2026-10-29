#include <cock/core/memory/shared_memory.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::core::syscall {

using memory::NO_SHARED_OBJECT;
using memory::SharedMemory;
using memory::VirtAddr;
using memory::VirtualMemoryManager;
using task::Scheduler;
using task::Thread;
using utils::Logger;

namespace {

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);

} // namespace

uintptr_t sys_module_index(uintptr_t name_ptr, uintptr_t length, uintptr_t,
						   uintptr_t, uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || length == 0 || length >= SharedMemory::MAX_NAME_LENGTH)
		return FAILURE;
	if (!current->getProcess()->getCapabilities().holdsGrantAuthority()) {
		Logger::trace("sys_module_index: PID %d holds no grant authority",
					  current->getProcess()->getId());
		return FAILURE;
	}
	if (!VirtualMemoryManager::isUserRangeValid(VirtAddr(name_ptr), length, false,
												current->getUserSpaceLimit()))
		return FAILURE;

	char name[SharedMemory::MAX_NAME_LENGTH] = {};
	memcpy(name, reinterpret_cast<const void *>(name_ptr), length);

	size_t module_index = 0;
	if (SharedMemory::findModule(name, &module_index) == NO_SHARED_OBJECT)
		return FAILURE;
	return module_index;
}

} // namespace cock::core::syscall
