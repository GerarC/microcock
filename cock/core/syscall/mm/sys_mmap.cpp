#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using core::memory::VirtualMemoryManager;
using core::memory::VMMPermission;
using core::task::Scheduler;
using utils::Logger;

constexpr uintptr_t MAP_PHYS = 0x1000;
constexpr size_t PAGE_SIZE = 0x1000;
constexpr uintptr_t MAP_ANONYMOUS = 0x20;

uintptr_t sys_mmap(uintptr_t addr, uintptr_t length, uintptr_t prot,
				   uintptr_t flags, uintptr_t fd, uintptr_t offset) {
	(void)prot;
	(void)fd;

	if (length == 0) return (uintptr_t)-1;
	size_t page_count = (length + PAGE_SIZE - 1) / PAGE_SIZE;

	if (flags & MAP_ANONYMOUS) {
		void *allocated = VirtualMemoryManager::allocPages(
			page_count, VMMPermission::USER_DATA);
		if (allocated) {
			Scheduler::getCurrentThread()->addMemoryRegion((uintptr_t)allocated,
														   page_count);
			return reinterpret_cast<uintptr_t>(allocated);
		}
		return (uintptr_t)-1;
	}

	if (flags & MAP_PHYS) {
		if (addr == 0) {
			Logger::error(
				"sys_mmap: MAP_PHYS requires a virtual 'addr' specific");
			return (uintptr_t)-1;
		}

		uintptr_t virt_addr = addr & ~(PAGE_SIZE - 1);
		uintptr_t phys_addr = offset & ~(PAGE_SIZE - 1);

		for (size_t i = 0; i < page_count; i++) {
			VirtualMemoryManager::map(phys_addr + (i * PAGE_SIZE),
									  virt_addr + (i * PAGE_SIZE),
									  VMMPermission::USER_DATA);
		}
		return virt_addr;
	}

	Logger::warn("sys_mmap: Not supported option (TODO MAP_PHYS)");
	return (uintptr_t)-1;
}

uintptr_t sys_munmap(uintptr_t addr, uintptr_t length, uintptr_t, uintptr_t, uintptr_t, uintptr_t) {
    if (length == 0 || addr % PAGE_SIZE != 0) return (uintptr_t)-1;
    
    size_t page_count = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    VirtualMemoryManager::freePages(reinterpret_cast<void*>(addr), page_count);
    Scheduler::getCurrentThread()->removeMemoryRegion(addr);
    
    return 0;
}

} // namespace cock::core::syscall
