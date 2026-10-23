#include <cock/abi/capability.hpp>
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

using abi::CapabilityType;
using memory::NO_SHARED_OBJECT;
using memory::SharedMemory;
using memory::VirtAddr;
using memory::VirtualMemoryManager;
using task::Process;
using task::Scheduler;
using task::Thread;
using utils::Logger;

namespace {

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uint64_t SINGLE_MODULE = 1;

} // namespace

uintptr_t sys_module_open(uintptr_t name_ptr, uintptr_t length, uintptr_t,
						  uintptr_t, uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || length == 0 || length >= SharedMemory::MAX_NAME_LENGTH)
		return FAILURE;
	if (!VirtualMemoryManager::isUserRangeValid(VirtAddr(name_ptr), length, false,
												current->getUserSpaceLimit()))
		return FAILURE;

	char name[SharedMemory::MAX_NAME_LENGTH] = {};
	memcpy(name, reinterpret_cast<const void *>(name_ptr), length);

	size_t module_index = 0;
	uint32_t object = SharedMemory::findModule(name, &module_index);
	if (object == NO_SHARED_OBJECT) return FAILURE;

	Process &process = *current->getProcess();
	if (!process.getCapabilities().covers(CapabilityType::BOOT_MODULE,
										  module_index, SINGLE_MODULE)) {
		Logger::trace("sys_module_open: PID %d holds no capability for module %u",
					  process.getId(), static_cast<unsigned>(module_index));
		return FAILURE;
	}

	return SharedMemory::attach(object, process.getId()) ? object : FAILURE;
}

} // namespace cock::core::syscall
