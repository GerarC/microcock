#include <cock/arch/x86/memory/paging.hpp>
#include <cock/arch/x86/memory/pmm.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/utils/logger.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::memory {

using arch::x86::PhysicalMemoryManager;
using arch::x86::NULL_PTR;
using arch::x86::PageFlag;
using utils::Logger;

static uintptr_t next_virtual_address = 0xD0000000;
static constexpr size_t PAGE_SIZE = 0x1000;

static uint32_t toX86Flag(VMMPermission permission) {
	uint32_t arch_flags = PageFlag::PRESENT;

	if (permission & VMMPermission::WRITE) arch_flags |= PageFlag::WRITEABLE;
	if (permission & VMMPermission::USER) arch_flags |= PageFlag::USER;

	return arch_flags;
}

void *VirtualMemoryManager::allocPages(size_t page_count, VMMPermission permission) {
	if (page_count == 0) return nullptr;

	uintptr_t start_virt = next_virtual_address;
	next_virtual_address += (page_count * PAGE_SIZE);

	for (size_t idx = 0; idx < page_count; idx++) {
		uintptr_t virt = start_virt + (idx * PAGE_SIZE);
		uint32_t phys = PhysicalMemoryManager::allocFrame();

		if (phys == NULL_PTR) {
			Logger::fatal("VMM: Out of memory");
			return nullptr;
		}

		VirtualMemoryManager::map(phys, virt, permission);
	}

	return reinterpret_cast<void *>(start_virt);
}

void VirtualMemoryManager::freePages(void *vaddr, size_t page_count) {
	uintptr_t virt = reinterpret_cast<uintptr_t>(vaddr);

	for (size_t idx = 0; idx < page_count; idx++) {
		uintptr_t current_virt = virt + (idx * PAGE_SIZE);
        uint32_t phys = PhysicalMemoryManager::getPhysicalAddress(current_virt);
        PhysicalMemoryManager::unmapPage(current_virt);
        if(phys != NULL_PTR) PhysicalMemoryManager::freeFrame(phys);
	}
}

void VirtualMemoryManager::map(uintptr_t phys, uintptr_t virt, VMMPermission permission){
    PhysicalMemoryManager::mapPage(phys, virt, toX86Flag(permission));
}

void VirtualMemoryManager::unmap(uintptr_t virt){
    PhysicalMemoryManager::unmapPage(virt);
}


} // namespace cock::core::memory
