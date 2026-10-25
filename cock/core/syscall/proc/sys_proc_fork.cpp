#include <cock/abi/call.hpp>
#include <cock/abi/capability.hpp>
#include <cock/core/memory/shared_memory.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/security/capability_inherit.hpp>
#include <cock/core/syscall/proc/pending_image.hpp>
#include <cock/core/syscall/proc/served_target.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/sync_ipc.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::core::syscall {

using memory::NO_SHARED_OBJECT;
using memory::PhysAddr;
using memory::SharedMemory;
using memory::VirtAddr;
using memory::VirtualMemoryManager;
using security::CapabilityType;
using task::Process;
using task::Scheduler;
using task::SyncIpc;
using task::TaskManager;
using task::Thread;
using utils::Logger;

namespace {

bool regions_may_be_inherited(const Process &parent) {
	for (size_t index = 0; index < parent.getRegionCount(); index++) {
		uint32_t object = parent.getRegion(index).shared_object;
		if (object != NO_SHARED_OBJECT && !SharedMemory::canInherit(object, parent.getId()))
			return false;
	}
	return true;
}

void hold_inherited_regions(const Process &parent, const Process &child) {
	for (size_t index = 0; index < parent.getRegionCount(); index++) {
		uint32_t object = parent.getRegion(index).shared_object;
		if (object != NO_SHARED_OBJECT)
			SharedMemory::inherit(object, parent.getId(), child.getId());
	}
}

} // namespace

uintptr_t sys_proc_fork(uintptr_t pid, uintptr_t reply_ptr, uintptr_t, uintptr_t, uintptr_t,
						uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (current == nullptr || pid > UINT32_MAX) return PROC_FAILURE;

	Process &creator = *current->getProcess();
	if (!creator.getCapabilities().covers(CapabilityType::SPAWN, abi::SPAWN_FIRST, abi::SPAWN_COUNT)) {
		Logger::trace("sys_proc_fork: PID %d holds no spawn capability", creator.getId());
		return PROC_FAILURE;
	}
	if (!VirtualMemoryManager::isUserRangeValid(VirtAddr(reply_ptr), sizeof(uint32_t[abi::call::WORD_COUNT]),
												false, current->getUserSpaceLimit()))
		return PROC_FAILURE;
	uint32_t words[abi::call::WORD_COUNT];
	memcpy(words, reinterpret_cast<const void *>(reply_ptr), sizeof(words));

	Thread *target = TaskManager::getThreadByPID(static_cast<uint32_t>(pid));
	if (!serves_main_thread(*current, target)) return PROC_FAILURE;
	Process &parent = *target->getProcess();

	if (!creator.canReserveExitNotice() || !TaskManager::hasFreePid()) return PROC_FAILURE;
	if (!security::can_inherit_capabilities(parent) || !regions_may_be_inherited(parent))
		return PROC_FAILURE;

	PhysAddr space = VirtualMemoryManager::cloneAddressSpace(parent.getAddressSpace());
	if (space.isNull()) return PROC_FAILURE;

	Process *child = Process::create(space, parent.getUserSpaceLimit());
	child->copyImageFrom(parent);
	child->setParentId(creator.getId());
	child->setNotifiesExit(true);

	Thread *thread = Thread::createClone(*child, *target);
	if (thread == nullptr) {
		delete child;
		return PROC_FAILURE;
	}

	creator.reserveExitNotice();
	hold_inherited_regions(parent, *child);
	if (!security::inherit_capabilities(parent, *child))
		utils::panic("fork: the capabilities changed during the call");

	SyncIpc::writeReply(thread->getStackPointer(), current->getId(), words);
	Scheduler::addThread(thread);
	return thread->getId();
}

} // namespace cock::core::syscall
