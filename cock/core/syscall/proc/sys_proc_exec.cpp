#include <cock/abi/capability.hpp>
#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/shared_memory.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/proc/pending_image.hpp>
#include <cock/core/syscall/proc/served_target.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/pending_process.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using memory::SharedMemory;
using memory::VirtAddr;
using memory::VirtualMemoryManager;
using memory::VMMPermission;
using security::CapabilityType;
using task::PendingProcesses;
using task::Process;
using task::Scheduler;
using task::TaskManager;
using task::Thread;
using task::ThreadState;
using utils::Logger;

namespace {

bool entry_is_mapped(const Process &process, uintptr_t entry) {
	if (!image_range_fits(process, entry, 1)) return false;
	PendingSpaceScope scope(process);
	return VirtualMemoryManager::isUserRangeValid(VirtAddr(entry), 1, false,
												  process.getUserSpaceLimit());
}

} // namespace

uintptr_t sys_proc_exec(uintptr_t handle, uintptr_t entry_point, uintptr_t pid, uintptr_t initial,
						uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	PendingProcesses::Entry *entry = pending_of_caller(handle);
	if (current == nullptr || entry == nullptr || pid > UINT32_MAX) return PROC_FAILURE;

	Process &creator = *current->getProcess();
	if (!creator.getCapabilities().covers(CapabilityType::SPAWN, abi::SPAWN_FIRST, abi::SPAWN_COUNT)) {
		Logger::trace("sys_proc_exec: PID %d holds no spawn capability", creator.getId());
		return PROC_FAILURE;
	}
	if (entry->hasRights || entry->notify) return PROC_FAILURE;

	Thread *target = TaskManager::getThreadByPID(static_cast<uint32_t>(pid));
	if (!serves_main_thread(*current, target) || !entry_is_mapped(*entry->process, entry_point))
		return PROC_FAILURE;

	Process *fresh = entry->process;
	VirtAddr limit = fresh->getUserSpaceLimit();
	VirtAddr stack_base(limit.raw() - task::USER_STACK_SIZE);
	{
		PendingSpaceScope scope(*fresh);
		if (VirtualMemoryManager::mapPagesAt(stack_base, task::USER_PAGES, VMMPermission::USER_DATA,
											 limit)
				.isNull())
			return PROC_FAILURE;
	}

	Process *replaced = target->getProcess();
	{
		hal::InterruptGuard guard;
		PendingProcesses::release(static_cast<uint32_t>(handle));
		TaskManager::terminateSiblings(*target);
		replaced->swapImage(*fresh);
		SharedMemory::dropProcess(*replaced);
		PendingProcesses::discardOwnedBy(replaced);
		target->restartAt(VirtAddr(entry_point), stack_base);
		hal::set_thread_argument(target->getStackPointer(), initial);
		Scheduler::addThread(target);
	}
	delete fresh;
	return PROC_SUCCESS;
}

} // namespace cock::core::syscall
