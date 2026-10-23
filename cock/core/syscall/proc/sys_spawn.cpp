#include <cock/abi/process.hpp>
#include <cock/abi/service.hpp>
#include <cock/core/loader/elf.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/security/capability_tree.hpp>
#include <cock/core/security/ipc_access.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <hal/page.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::core::syscall {

using loader::LoadedImage;
using memory::PhysAddr;
using memory::VirtAddr;
using memory::VirtualMemoryManager;
using security::capability_tree;
using security::Capability;
using security::CapabilityType;
using security::SINGLE_LABEL;
using task::Process;
using task::Scheduler;
using task::TaskManager;
using task::Thread;
using task::ThreadPriority;
using utils::Logger;

namespace {

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr size_t MAX_IMAGE_BYTES = 0x400000;

}

static bool read_rights(const Process &spawner, VirtAddr limit, uintptr_t rights_ptr,
						abi::service::SpawnRights &rights) {
	if (!VirtualMemoryManager::isUserRangeValid(VirtAddr(rights_ptr), sizeof(rights), false,
												limit))
		return false;
	memcpy(&rights, reinterpret_cast<const void *>(rights_ptr), sizeof(rights));
	if (rights.label == abi::service::NO_SERVICE ||
		rights.ipcCount > abi::service::MAX_SPAWN_IPC ||
		!capability_tree.hasRoom(rights.ipcCount))
		return false;

	for (uint32_t index = 0; index < rights.ipcCount; index++) {
		const Capability *source = spawner.getCapabilities().grantable(
			CapabilityType::IPC, rights.ipc[index], SINGLE_LABEL);
		if (source == nullptr || capability_tree.find(source->node) == nullptr) return false;
	}
	return true;
}

static void give_rights(const Process &spawner, Process &process,
						const abi::service::SpawnRights &rights) {
	process.setServiceId(rights.label);
	for (uint32_t index = 0; index < rights.ipcCount; index++) {
		const Capability *source = spawner.getCapabilities().grantable(
			CapabilityType::IPC, rights.ipc[index], SINGLE_LABEL);
		Capability granted;
		granted.type = CapabilityType::IPC;
		granted.first = rights.ipc[index];
		granted.count = SINGLE_LABEL;
		granted.node = capability_tree.derive(source->node, process.getId(),
											  CapabilityType::IPC, granted.first, granted.count);
		process.getCapabilities().add(granted);
	}
}

uintptr_t sys_spawn(uintptr_t image_ptr, uintptr_t image_size, uintptr_t name_ptr,
					uintptr_t name_length, uintptr_t flags, uintptr_t rights_ptr) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current) return FAILURE;
	if (!current->getProcess()->getCapabilities().holdsGrantAuthority()) {
		Logger::trace("sys_spawn: PID %d holds no grant authority",
					  current->getProcess()->getId());
		return FAILURE;
	}
	if (image_size == 0 || image_size > MAX_IMAGE_BYTES || name_length == 0 ||
		name_length >= Process::MAX_NAME_LENGTH)
		return FAILURE;

	VirtAddr limit = current->getUserSpaceLimit();
	if (!VirtualMemoryManager::isUserRangeValid(VirtAddr(image_ptr), image_size, false,
												limit) ||
		!VirtualMemoryManager::isUserRangeValid(VirtAddr(name_ptr), name_length, false,
												limit))
		return FAILURE;
	abi::service::SpawnRights rights = {};
	bool has_rights = rights_ptr != 0;
	if (has_rights && !read_rights(*current->getProcess(), limit, rights_ptr, rights)) {
		Logger::trace("sys_spawn: PID %d asked for rights it cannot give",
					  current->getProcess()->getId());
		return FAILURE;
	}
	if (!TaskManager::hasFreePid()) return FAILURE;
	bool notify = (flags & abi::process::SPAWN_NOTIFY_EXIT) != 0;
	if (notify && !current->getProcess()->canReserveExitNotice()) {
		Logger::trace("sys_spawn: PID %d has too many children with exit notices",
					  current->getProcess()->getId());
		return FAILURE;
	}

	char name[Process::MAX_NAME_LENGTH] = {};
	memcpy(name, reinterpret_cast<const void *>(name_ptr), name_length);

	size_t pages = (image_size + hal::PAGE_MASK) >> hal::PAGE_SHIFT;
	void *copy = VirtualMemoryManager::allocPages(pages);
	if (copy == nullptr) return FAILURE;
	memcpy(copy, reinterpret_cast<const void *>(image_ptr), image_size);

	if (!loader::has_elf_magic(copy, image_size)) {
		VirtualMemoryManager::freePages(copy, pages);
		return FAILURE;
	}

	PhysAddr address_space = VirtualMemoryManager::createAddressSpace();
	LoadedImage image;
	bool loaded = !address_space.isNull() &&
				  loader::load_executable(copy, image_size, address_space, image);
	VirtualMemoryManager::freePages(copy, pages);
	if (!loaded) {
		if (!address_space.isNull())
			VirtualMemoryManager::destroyAddressSpace(address_space);
		return FAILURE;
	}

	Process *process = Process::create(address_space, image.user_space_limit);
	process->setName(name);
	process->setParentId(current->getProcess()->getId());
	process->setNotifiesExit(notify);
	if (notify) current->getProcess()->reserveExitNotice();
	Thread *thread = new Thread(*process, image.entry, ThreadPriority::NORMAL);
	if (has_rights) give_rights(*current->getProcess(), *process, rights);
	Scheduler::addThread(thread);
	return thread->getId();
}

} // namespace cock::core::syscall
