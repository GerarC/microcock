#include <cock/arch/x86/memory/layout.hpp>
#include <cock/arch/x86/memory/paging.hpp>
#include <cock/arch/x86/memory/pmm.hpp>
#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace cock::core::memory {

using arch::x86::KERNEL_HEAP_BASE;
using arch::x86::KERNEL_HEAP_LIMIT;
using arch::x86::NULL_PTR;
using arch::x86::PAGE_MASK;
using arch::x86::PAGE_SIZE;
using arch::x86::PageFlag;
using arch::x86::PhysicalMemoryManager;
using arch::x86::USER_TOP;
using hal::InterruptGuard;
using utils::Logger;

static_assert(USER_SPACE_LIMIT == USER_TOP,
			  "core and arch disagree about the user space limit");

namespace {

constexpr size_t MAX_RECYCLED_STACKS = 64;
constexpr uint64_t ADDRESS_SPACE_END = 0x100000000ULL;

struct RecycledStack {
	uintptr_t base;
	size_t pages;
};

RecycledStack recycledStacks[MAX_RECYCLED_STACKS];
size_t recycledCount = 0;
uintptr_t nextKernelVaddr = KERNEL_HEAP_BASE;

uint32_t toX86Flag(VMMPermission permission) {
	uint32_t arch_flags = PageFlag::PRESENT;

	if (permission & VMMPermission::WRITE) arch_flags |= PageFlag::WRITEABLE;
	if (permission & VMMPermission::USER) arch_flags |= PageFlag::USER;
	if (permission & VMMPermission::DEVICE)
		arch_flags |= PageFlag::NOT_OWNED | PageFlag::PCD;

	return arch_flags;
}

bool reserveKernelRange(size_t pages, uintptr_t &out) {
	if (pages == 0) return false;
	if (pages > (KERNEL_HEAP_LIMIT - nextKernelVaddr) / PAGE_SIZE) return false;

	out = nextKernelVaddr;
	nextKernelVaddr += pages * PAGE_SIZE;
	return true;
}

void releasePages(uintptr_t virt, size_t pages) {
	for (size_t idx = 0; idx < pages; idx++) {
		uintptr_t current = virt + idx * PAGE_SIZE;
		uint32_t phys = PhysicalMemoryManager::getPhysicalAddress(current);
		uint32_t flags = PhysicalMemoryManager::getPageFlags(current);
		PhysicalMemoryManager::unmapPage(current);
		if (phys != NULL_PTR && !(flags & PageFlag::NOT_OWNED))
			PhysicalMemoryManager::freeFrame(phys);
	}
}

bool mapFresh(uintptr_t virt, size_t pages, uint32_t flags) {
	for (size_t idx = 0; idx < pages; idx++) {
		uintptr_t current = virt + idx * PAGE_SIZE;
		uint32_t phys = PhysicalMemoryManager::allocFrame();

		if (phys == NULL_PTR ||
			!PhysicalMemoryManager::mapPage(phys, current, flags)) {
			if (phys != NULL_PTR) PhysicalMemoryManager::freeFrame(phys);
			releasePages(virt, idx);
			return false;
		}
		memset(reinterpret_cast<void *>(current), 0, PAGE_SIZE);
	}
	return true;
}

void recycleStack(uintptr_t base, size_t pages) {
	if (recycledCount < MAX_RECYCLED_STACKS)
		recycledStacks[recycledCount++] = {base, pages};
}

} // namespace

void *VirtualMemoryManager::allocPages(size_t page_count,
									   VMMPermission permission) {
	if (page_count == 0 || (permission & VMMPermission::USER)) return nullptr;

	InterruptGuard guard;
	uintptr_t start_virt = 0;
	if (!reserveKernelRange(page_count, start_virt) ||
		!mapFresh(start_virt, page_count, toX86Flag(permission))) {
		Logger::fatal("VMM: Out of memory");
		return nullptr;
	}

	return reinterpret_cast<void *>(start_virt);
}

void VirtualMemoryManager::freePages(void *vaddr, size_t page_count) {
	InterruptGuard guard;
	releasePages(reinterpret_cast<uintptr_t>(vaddr), page_count);
}

void *VirtualMemoryManager::allocStack(size_t page_count) {
	if (page_count == 0) return nullptr;

	InterruptGuard guard;
	uintptr_t base = 0;

	for (size_t idx = 0; idx < recycledCount; idx++) {
		if (recycledStacks[idx].pages != page_count) continue;
		base = recycledStacks[idx].base;
		recycledStacks[idx] = recycledStacks[--recycledCount];
		break;
	}

	if (base == 0) {
		uintptr_t region = 0;
		if (!reserveKernelRange(page_count + 1, region)) return nullptr;
		base = region + PAGE_SIZE;
	}

	if (!mapFresh(base, page_count, PageFlag::PRESENT | PageFlag::WRITEABLE)) {
		recycleStack(base, page_count);
		return nullptr;
	}

	return reinterpret_cast<void *>(base);
}

void VirtualMemoryManager::freeStack(void *base, size_t page_count) {
	if (!base) return;

	InterruptGuard guard;
	uintptr_t virt = reinterpret_cast<uintptr_t>(base);
	releasePages(virt, page_count);
	recycleStack(virt, page_count);
}

bool VirtualMemoryManager::map(uintptr_t phys, uintptr_t virt,
							   VMMPermission permission) {
	return PhysicalMemoryManager::mapPage(phys, virt, toX86Flag(permission));
}

void VirtualMemoryManager::unmap(uintptr_t virt) {
	PhysicalMemoryManager::unmapPage(virt);
}

uintptr_t VirtualMemoryManager::createAddressSpace() {
	return PhysicalMemoryManager::createAddressSpace();
}

void VirtualMemoryManager::destroyAddressSpace(uintptr_t address_space) {
	PhysicalMemoryManager::destroyAddressSpace(address_space);
}

uintptr_t VirtualMemoryManager::getKernelDirectory() {
	return PhysicalMemoryManager::getKernelDirectory();
}

void *VirtualMemoryManager::mapPagesAt(uintptr_t vaddr, size_t num_pages,
									   VMMPermission permission,
									   uintptr_t user_limit) {
	if (num_pages == 0) return nullptr;

	uintptr_t start = vaddr & ~PAGE_MASK;
	uint64_t end = static_cast<uint64_t>(start) +
				   static_cast<uint64_t>(num_pages) * PAGE_SIZE;
	if (end > ADDRESS_SPACE_END) return nullptr;
	if (permission & VMMPermission::USER) {
		if (user_limit == 0 || user_limit > USER_SPACE_LIMIT) return nullptr;
		if (end > user_limit) return nullptr;
	}

	uint32_t flags = toX86Flag(permission);
	InterruptGuard guard;

	for (size_t idx = 0; idx < num_pages; idx++) {
		uintptr_t virt = start + idx * PAGE_SIZE;
		uint32_t existing = PhysicalMemoryManager::getPageFlags(virt);

		if (existing != 0) {
			uint32_t merged = existing | flags;
			if (merged != existing &&
				!PhysicalMemoryManager::updatePageFlags(virt, merged))
				return nullptr;
			continue;
		}

		if (!mapFresh(virt, 1, flags)) {
			Logger::fatal("VMM: Out of memory");
			return nullptr;
		}
	}

	return reinterpret_cast<void *>(start);
}

bool VirtualMemoryManager::isUserRangeValid(uintptr_t ptr, size_t length,
											bool write, uintptr_t user_limit) {
	if (user_limit == 0 || user_limit > USER_SPACE_LIMIT) return false;
	if (length > ADDRESS_SPACE_END - 1) return false;
	if (ptr > user_limit) return false;
	if (static_cast<uint64_t>(ptr) + length > user_limit) return false;
	return PhysicalMemoryManager::isRangeAccessible(ptr, length, true, write);
}

uintptr_t VirtualMemoryManager::getPhysicalAddress(uintptr_t virt) {
    return PhysicalMemoryManager::getPhysicalAddress(virt);
}

} // namespace cock::core::memory
