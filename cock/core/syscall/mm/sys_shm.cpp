#include <cock/core/memory/shared_memory.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/mm/placement.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <hal/page.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using memory::NO_SHARED_OBJECT;
using memory::SharedMemory;
using memory::VirtAddr;
using memory::VirtualMemoryManager;
using memory::VMMPermission;
using task::Process;
using task::Scheduler;
using task::TaskManager;
using task::Thread;
using utils::Logger;

namespace {

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uintptr_t SUCCESS = 0;

bool flag(uintptr_t value) { return value != 0; }

} // namespace

uintptr_t sys_shm_create(uintptr_t pages, uintptr_t, uintptr_t, uintptr_t,
						 uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || pages == 0 || pages > SharedMemory::MAX_OBJECT_PAGES)
		return FAILURE;

	uint32_t id = SharedMemory::create(*current->getProcess(), pages);
	return id == NO_SHARED_OBJECT ? FAILURE : id;
}

uintptr_t sys_shm_share(uintptr_t id, uintptr_t target_pid, uintptr_t writable,
						uintptr_t, uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || id > UINT32_MAX || target_pid > UINT32_MAX) return FAILURE;

	Thread *target =
		TaskManager::getThreadByPID(static_cast<uint32_t>(target_pid));
	if (!target || target->getProcess()->isKernel() ||
		target->getProcess()->getId() != target_pid)
		return FAILURE;

	if (!SharedMemory::share(static_cast<uint32_t>(id),
							 current->getProcess()->getId(),
							 static_cast<uint32_t>(target_pid), flag(writable))) {
		Logger::trace("sys_shm_share: PID %d may not share object %u",
					  current->getProcess()->getId(), static_cast<unsigned>(id));
		return FAILURE;
	}
	return SUCCESS;
}

uintptr_t sys_shm_map(uintptr_t id, uintptr_t writable, uintptr_t, uintptr_t,
					  uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || id > UINT32_MAX) return FAILURE;

	Process &process = *current->getProcess();
	uint32_t object = static_cast<uint32_t>(id);
	if (!SharedMemory::mayMap(object, process.getId(), flag(writable))) {
		Logger::trace("sys_shm_map: PID %d may not map object %u",
					  process.getId(), static_cast<unsigned>(id));
		return FAILURE;
	}

	size_t pages = SharedMemory::pageCount(object);
	if (pages == 0 || !process.hasRegionSlot()) return FAILURE;

	VirtAddr base = find_user_placement(*current, pages);
	if (base.isNull()) return FAILURE;

	VMMPermission permission =
		VMMPermission::READ | VMMPermission::USER | VMMPermission::SHARED;
	if (flag(writable)) permission = permission | VMMPermission::WRITE;

	for (size_t idx = 0; idx < pages; idx++) {
		if (!VirtualMemoryManager::map(SharedMemory::frame(object, idx),
									   base + (idx << hal::PAGE_SHIFT),
									   permission)) {
			VirtualMemoryManager::freePages(base.as<void>(), idx);
			return FAILURE;
		}
	}

	if (!process.addMemoryRegion(base, pages, false, object)) {
		VirtualMemoryManager::freePages(base.as<void>(), pages);
		return FAILURE;
	}
	SharedMemory::mapped(object);
	return base.raw();
}

uintptr_t sys_shm_release(uintptr_t id, uintptr_t, uintptr_t, uintptr_t,
						  uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || id > UINT32_MAX) return FAILURE;

	return SharedMemory::release(static_cast<uint32_t>(id),
								 current->getProcess()->getId())
			   ? SUCCESS
			   : FAILURE;
}

} // namespace cock::core::syscall
