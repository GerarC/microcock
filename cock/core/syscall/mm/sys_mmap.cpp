#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <hal/address_space.hpp>
#include <hal/page.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using core::memory::VirtualMemoryManager;
using core::memory::VMMPermission;
using core::task::Scheduler;
using core::task::Thread;
using utils::Logger;

constexpr uintptr_t MAP_PHYS = 0x1000;
constexpr uintptr_t MAP_ANONYMOUS = 0x20;
constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uintptr_t LEGACY_VIDEO_START = 0xA0000;
constexpr uintptr_t LEGACY_VIDEO_END = 0xC0000;
constexpr uintptr_t MIN_USER_ADDRESS = hal::PAGE_SIZE;

static bool pages_for(uintptr_t length, size_t &pages) {
	if (length == 0 || length > UINTPTR_MAX - hal::PAGE_MASK) return false;
	pages = (length + hal::PAGE_MASK) >> hal::PAGE_SHIFT;
	return true;
}

static uintptr_t image_limit(const Thread &thread) {
	return thread.getUserSpaceLimit() - task::USER_STACK_SIZE;
}

static uintptr_t map_anonymous(Thread &current, size_t pages) {
	uintptr_t base = current.getMmapCursor();
	if (pages > (image_limit(current) - base) >> hal::PAGE_SHIFT)
		return FAILURE;

	void *mapped = VirtualMemoryManager::mapPagesAt(
		base, pages, VMMPermission::USER_DATA, current.getUserSpaceLimit());
	if (!mapped) return FAILURE;

	current.setMmapCursor(base + (pages << hal::PAGE_SHIFT));
	current.addMemoryRegion(base, pages);
	return base;
}

static uintptr_t map_physical(Thread &current, uintptr_t addr, size_t pages,
							  uintptr_t offset) {
	if (addr == 0) {
		Logger::error("sys_mmap: MAP_PHYS requires a virtual 'addr' specific");
		return FAILURE;
	}

	uintptr_t virt = addr & ~hal::PAGE_MASK;
	uintptr_t phys = offset & ~hal::PAGE_MASK;
	size_t span = pages << hal::PAGE_SHIFT;

	if (phys < LEGACY_VIDEO_START || phys > LEGACY_VIDEO_END ||
		span > LEGACY_VIDEO_END - phys) {
		Logger::warn("sys_mmap: PID %d tried to map physical 0x%x outside the "
					 "video window",
					 current.getId(), phys);
		return FAILURE;
	}
	if (virt < MIN_USER_ADDRESS || virt > image_limit(current) ||
		span > image_limit(current) - virt)
		return FAILURE;

	for (size_t idx = 0; idx < pages; idx++) {
		uintptr_t step = idx << hal::PAGE_SHIFT;
		if (!VirtualMemoryManager::map(
				phys + step, virt + step,
				VMMPermission::USER_DATA | VMMPermission::DEVICE)) {
			VirtualMemoryManager::freePages(reinterpret_cast<void *>(virt), idx);
			return FAILURE;
		}
	}
	current.addMemoryRegion(virt, pages);
	return virt;
}

uintptr_t sys_mmap(uintptr_t addr, uintptr_t length, uintptr_t prot,
				   uintptr_t flags, uintptr_t fd, uintptr_t offset) {
	(void)prot;
	(void)fd;

	Thread *current = Scheduler::getCurrentThread();
	size_t pages = 0;
	if (!current || !pages_for(length, pages)) return FAILURE;

	if (flags & MAP_ANONYMOUS) return map_anonymous(*current, pages);
	if (flags & MAP_PHYS) return map_physical(*current, addr, pages, offset);

	Logger::warn("sys_mmap: Not supported option");
	return FAILURE;
}

uintptr_t sys_munmap(uintptr_t addr, uintptr_t length, uintptr_t, uintptr_t,
					 uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	size_t pages = 0;
	if (!current || (addr & hal::PAGE_MASK) != 0 || !pages_for(length, pages))
		return FAILURE;

	if (!current->takeMemoryRegion(addr, pages)) return FAILURE;
	VirtualMemoryManager::freePages(reinterpret_cast<void *>(addr), pages);
	return 0;
}

} // namespace cock::core::syscall
