#include <cock/abi/process.hpp>
#include <cock/core/loader/elf.hpp>
#include <cock/core/memory/vmm.hpp>
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

uintptr_t sys_spawn(uintptr_t image_ptr, uintptr_t image_size, uintptr_t name_ptr,
					uintptr_t name_length, uintptr_t flags, uintptr_t) {
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
	if (!TaskManager::hasFreePid()) return FAILURE;

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
	process->setNotifiesExit((flags & abi::process::SPAWN_NOTIFY_EXIT) != 0);
	Thread *thread = new Thread(*process, image.entry, ThreadPriority::NORMAL);
	Scheduler::addThread(thread);
	return thread->getId();
}

} // namespace cock::core::syscall
