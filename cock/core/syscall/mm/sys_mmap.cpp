#include <cock/core/cock.hpp>
#include <cock/core/memory/shared_memory.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/security/capability.hpp>
#include <cock/core/syscall/mm/placement.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <hal/address_space.hpp>
#include <hal/console.hpp>
#include <hal/page.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using core::memory::NO_SHARED_OBJECT;
using core::memory::PhysAddr;
using core::memory::SharedMemory;
using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;
using core::memory::VMMPermission;
using core::security::CapabilityType;
using core::task::Process;
using core::task::Scheduler;
using core::task::Thread;
using utils::Logger;

constexpr uintptr_t MAP_PHYS = 0x1000;
constexpr uintptr_t MAP_ANONYMOUS = 0x20;
constexpr uintptr_t PROT_WRITE = 0x02;
constexpr uintptr_t PROT_KNOWN = 0x07;
constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uintptr_t MIN_USER_ADDRESS = hal::PAGE_SIZE;

static bool pages_for(uintptr_t length, size_t &pages) {
	if (length == 0 || length > UINTPTR_MAX - hal::PAGE_MASK) return false;
	pages = (length + hal::PAGE_MASK) >> hal::PAGE_SHIFT;
	return true;
}

static bool protect_read_only(VirtAddr base, size_t pages) {
	for (size_t idx = 0; idx < pages; idx++) {
		if (!VirtualMemoryManager::protectPage(base + (idx << hal::PAGE_SHIFT),
											   VMMPermission::USER_READONLY))
			return false;
	}
	return true;
}

static uintptr_t map_anonymous(Thread &current, size_t pages, bool writable) {
	Process &process = *current.getProcess();
	if (!process.hasRegionSlot() || !process.canOwnPages(pages)) return FAILURE;

	VirtAddr base = find_user_placement(current, pages);
	if (base.isNull()) return FAILURE;

	VirtAddr mapped = VirtualMemoryManager::mapPagesAt(
		base, pages, VMMPermission::USER_DATA, current.getUserSpaceLimit());
	if (mapped.isNull()) return FAILURE;

	if (!writable && !protect_read_only(base, pages)) {
		VirtualMemoryManager::freePages(base.as<void>(), pages);
		return FAILURE;
	}

	if (!process.addMemoryRegion(base, pages, true)) {
		VirtualMemoryManager::freePages(base.as<void>(), pages);
		return FAILURE;
	}
	return base.raw();
}

static void claim_screen_if_mapped(Thread &current, PhysAddr phys, size_t span) {
	if (cock::debug_output_pid != 0) return;
	if (phys.raw() >= hal::CONSOLE_SCREEN_BASE + hal::CONSOLE_SCREEN_SIZE ||
		phys.raw() + span <= hal::CONSOLE_SCREEN_BASE)
		return;
	hal::console_detach();
	cock::debug_output_pid = current.getId();
	Logger::info("Screen mapped by PID %d, kernel console detached", current.getId());
}

static uintptr_t map_physical(Thread &current, uintptr_t addr, size_t pages,
							  uintptr_t offset, bool writable) {
	if (addr == 0) {
		Logger::trace("sys_mmap: MAP_PHYS requires a fixed virtual address");
		return FAILURE;
	}

	if (!current.getProcess()->hasRegionSlot()) return FAILURE;

	VirtAddr virt = VirtAddr(addr).pageBase();
	PhysAddr phys = PhysAddr(static_cast<hal::PhysicalRaw>(offset)).pageBase();
	size_t span = pages << hal::PAGE_SHIFT;

	if (!current.getProcess()->getCapabilities().covers(
			CapabilityType::PHYSICAL_MEMORY, phys.raw(), span) ||
		!VirtualMemoryManager::isDeviceMemory(phys, span)) {
		Logger::trace("sys_mmap: PID %d may not map physical 0x%x",
					 current.getId(), phys.raw());
		return FAILURE;
	}
	if (virt.raw() < MIN_USER_ADDRESS || virt.raw() > user_image_limit(current) ||
		span > user_image_limit(current) - virt.raw())
		return FAILURE;

	VMMPermission access =
		(writable ? VMMPermission::USER_DATA : VMMPermission::USER_READONLY) |
		VMMPermission::DEVICE;
	for (size_t idx = 0; idx < pages; idx++) {
		size_t step = idx << hal::PAGE_SHIFT;
		if (!VirtualMemoryManager::map(phys + step, virt + step, access)) {
			VirtualMemoryManager::freePages(virt.as<void>(), idx);
			return FAILURE;
		}
	}
	if (!current.getProcess()->addMemoryRegion(virt, pages, false,
											   memory::NO_SHARED_OBJECT, phys.raw())) {
		VirtualMemoryManager::freePages(virt.as<void>(), pages);
		return FAILURE;
	}
	claim_screen_if_mapped(current, phys, span);
	return virt.raw();
}

uintptr_t sys_mmap(uintptr_t addr, uintptr_t length, uintptr_t prot,
				   uintptr_t flags, uintptr_t fd, uintptr_t offset) {
	(void)fd;

	Thread *current = Scheduler::getCurrentThread();
	size_t pages = 0;
	if (!current || !pages_for(length, pages)) return FAILURE;
	if (prot == 0 || (prot & ~PROT_KNOWN) != 0) return FAILURE;

	bool writable = (prot & PROT_WRITE) != 0;
	if (flags & MAP_ANONYMOUS) return map_anonymous(*current, pages, writable);
	if (flags & MAP_PHYS)
		return map_physical(*current, addr, pages, offset, writable);

	Logger::trace("sys_mmap: not a supported option");
	return FAILURE;
}

uintptr_t sys_munmap(uintptr_t addr, uintptr_t length, uintptr_t, uintptr_t,
					 uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	size_t pages = 0;
	if (!current || (addr & hal::PAGE_MASK) != 0 || !pages_for(length, pages))
		return FAILURE;

	VirtAddr region(addr);
	uint32_t shared_object = NO_SHARED_OBJECT;
	if (!current->getProcess()->takeMemoryRegion(region, pages, &shared_object))
		return FAILURE;
	VirtualMemoryManager::freePages(region.as<void>(), pages);
	if (shared_object != NO_SHARED_OBJECT) SharedMemory::unmapped(shared_object);
	return 0;
}

} // namespace cock::core::syscall
