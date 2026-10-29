#include <cock/abi/executable.hpp>
#include <cock/abi/process.hpp>
#include <cock/abi/service.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/proc/pending_image.hpp>
#include <cock/core/syscall/proc/spawn_rights.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/pending_process.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <hal/address_space.hpp>
#include <hal/page.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::core::syscall {

using memory::PhysAddr;
using memory::VirtAddr;
using memory::VirtualMemoryManager;
using security::CapabilityType;
using task::PendingProcesses;
using task::Process;
using task::Scheduler;
using task::TaskManager;
using task::Thread;
using utils::Logger;

static_assert(abi::executable::USER_SPACE_LIMIT == hal::USER_SPACE_LIMIT_ELF32,
			  "the user loader and the kernel must agree on the user space limit");

uintptr_t sys_proc_create(uintptr_t name_ptr, uintptr_t name_length, uintptr_t flags,
						  uintptr_t rights_ptr, uintptr_t user_limit, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current) return PROC_FAILURE;

	Process &creator = *current->getProcess();
	if (!creator.getCapabilities().covers(CapabilityType::SPAWN, abi::SPAWN_FIRST,
										  abi::SPAWN_COUNT)) {
		Logger::trace("sys_proc_create: PID %d holds no spawn capability", creator.getId());
		return PROC_FAILURE;
	}
	if (name_length == 0 || name_length >= Process::MAX_NAME_LENGTH) return PROC_FAILURE;
	if ((flags & ~abi::process::SPAWN_NOTIFY_EXIT) != 0) return PROC_FAILURE;
	if ((user_limit & hal::PAGE_MASK) != 0 || user_limit > memory::USER_SPACE_LIMIT ||
		user_limit <= task::USER_STACK_SIZE + hal::PAGE_SIZE)
		return PROC_FAILURE;

	VirtAddr limit = current->getUserSpaceLimit();
	if (!VirtualMemoryManager::isUserRangeValid(VirtAddr(name_ptr), name_length, false, limit))
		return PROC_FAILURE;

	abi::service::SpawnRights rights = {};
	bool has_rights = rights_ptr != 0;
	if (has_rights && !read_spawn_rights(creator, limit, rights_ptr, rights)) {
		Logger::trace("sys_proc_create: PID %d asked for rights it cannot give", creator.getId());
		return PROC_FAILURE;
	}

	bool notify = (flags & abi::process::SPAWN_NOTIFY_EXIT) != 0;
	if (notify && !creator.canReserveExitNotice()) {
		Logger::trace("sys_proc_create: PID %d has too many children with exit notices",
					  creator.getId());
		return PROC_FAILURE;
	}
	if (!TaskManager::hasFreePid() || !PendingProcesses::canAdd(&creator)) return PROC_FAILURE;

	char name[Process::MAX_NAME_LENGTH] = {};
	memcpy(name, reinterpret_cast<const void *>(name_ptr), name_length);

	PhysAddr address_space = VirtualMemoryManager::createAddressSpace();
	if (address_space.isNull()) return PROC_FAILURE;

	Process *process = Process::create(address_space, VirtAddr(user_limit));
	process->setName(name);
	process->setParentId(creator.getId());
	uint32_t handle =
		PendingProcesses::add(&creator, process, notify, has_rights ? &rights : nullptr);
	if (handle == PendingProcesses::NO_HANDLE) {
		delete process;
		return PROC_FAILURE;
	}
	return handle;
}

} // namespace cock::core::syscall
