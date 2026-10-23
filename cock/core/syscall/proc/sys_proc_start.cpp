#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/proc/pending_image.hpp>
#include <cock/core/syscall/proc/spawn_rights.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/pending_process.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using memory::VirtAddr;
using memory::VirtualMemoryManager;
using memory::VMMPermission;
using task::PendingProcesses;
using task::Process;
using task::Scheduler;
using task::TaskManager;
using task::Thread;
using task::ThreadPriority;
using utils::Logger;

namespace {

constexpr size_t ENTRY_PROBE_BYTES = 1;

bool entry_is_mapped(const Process &process, uintptr_t entry) {
	if (!image_range_fits(process, entry, ENTRY_PROBE_BYTES)) return false;
	PendingSpaceScope scope(process);
	return VirtualMemoryManager::isUserRangeValid(VirtAddr(entry), ENTRY_PROBE_BYTES, false,
												  process.getUserSpaceLimit());
}

} // namespace

uintptr_t sys_proc_start(uintptr_t handle, uintptr_t entry_point, uintptr_t, uintptr_t,
						 uintptr_t, uintptr_t) {
	PendingProcesses::Entry *entry = pending_of_caller(handle);
	if (entry == nullptr || !entry_is_mapped(*entry->process, entry_point)) return PROC_FAILURE;

	Process &creator = *Scheduler::getCurrentThread()->getProcess();
	if (entry->hasRights && !spawn_rights_valid(creator, entry->rights)) {
		Logger::trace("sys_proc_start: PID %d can no longer give the rights", creator.getId());
		return PROC_FAILURE;
	}
	if (!TaskManager::hasFreePid()) return PROC_FAILURE;
	if (entry->notify && !creator.canReserveExitNotice()) {
		Logger::trace("sys_proc_start: PID %d has too many children with exit notices",
					  creator.getId());
		return PROC_FAILURE;
	}

	Process *process = entry->process;
	VirtAddr limit = process->getUserSpaceLimit();
	VirtAddr stack_base(limit.raw() - task::USER_STACK_SIZE);
	{
		PendingSpaceScope scope(*process);
		if (VirtualMemoryManager::mapPagesAt(stack_base, task::USER_PAGES,
											 VMMPermission::USER_DATA, limit)
				.isNull())
			return PROC_FAILURE;
	}

	Thread *thread = Thread::createUser(*process, VirtAddr(entry_point), stack_base,
										task::USER_STACK_SIZE, ThreadPriority::NORMAL);
	if (thread == nullptr) return PROC_FAILURE;

	process->setNotifiesExit(entry->notify);
	if (entry->notify) creator.reserveExitNotice();
	if (entry->hasRights) give_spawn_rights(creator, *process, entry->rights);
	PendingProcesses::release(static_cast<uint32_t>(handle));
	Scheduler::addThread(thread);
	return thread->getId();
}

} // namespace cock::core::syscall
