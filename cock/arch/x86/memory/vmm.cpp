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
using arch::x86::PAGE_MASK;
using arch::x86::PAGE_SIZE;
using arch::x86::PageFlag;
using arch::x86::Paging;
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

void releasePages(VirtAddr virt, size_t pages) {
	for (size_t idx = 0; idx < pages; idx++) {
		VirtAddr current = virt + idx * PAGE_SIZE;
		PhysAddr phys = Paging::translate(current);
		uint32_t flags = Paging::getPageFlags(current);
		Paging::unmapPage(current);
		if (!phys.isNull() && !(flags & PageFlag::NOT_OWNED))
			PhysicalMemoryManager::freeFrame(phys);
	}
}

bool mapFresh(VirtAddr virt, size_t pages, uint32_t flags) {
	for (size_t idx = 0; idx < pages; idx++) {
		VirtAddr current = virt + idx * PAGE_SIZE;
		PhysAddr phys = PhysicalMemoryManager::allocFrame();

		if (phys.isNull() || !Paging::mapPage(phys, current, flags)) {
			if (!phys.isNull()) PhysicalMemoryManager::freeFrame(phys);
			releasePages(virt, idx);
			return false;
		}
		memset(current.as<void>(), 0, PAGE_SIZE);
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
		!mapFresh(VirtAddr(start_virt), page_count, toX86Flag(permission))) {
		Logger::fatal("VMM: Out of memory");
		return nullptr;
	}

	return reinterpret_cast<void *>(start_virt);
}

void VirtualMemoryManager::freePages(void *vaddr, size_t page_count) {
	InterruptGuard guard;
	releasePages(VirtAddr::of(vaddr), page_count);
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

	if (!mapFresh(VirtAddr(base), page_count,
				  PageFlag::PRESENT | PageFlag::WRITEABLE)) {
		recycleStack(base, page_count);
		return nullptr;
	}

	return reinterpret_cast<void *>(base);
}

void VirtualMemoryManager::freeStack(void *base, size_t page_count) {
	if (!base) return;

	InterruptGuard guard;
	VirtAddr virt = VirtAddr::of(base);
	releasePages(virt, page_count);
	recycleStack(virt.raw(), page_count);
}

bool VirtualMemoryManager::map(PhysAddr phys, VirtAddr virt,
							   VMMPermission permission) {
	return Paging::mapPage(phys, virt, toX86Flag(permission));
}

void VirtualMemoryManager::unmap(VirtAddr virt) { Paging::unmapPage(virt); }

PhysAddr VirtualMemoryManager::createAddressSpace() {
	return Paging::createAddressSpace();
}

void VirtualMemoryManager::destroyAddressSpace(PhysAddr address_space) {
	Paging::destroyAddressSpace(address_space);
}

PhysAddr VirtualMemoryManager::getKernelDirectory() {
	return Paging::kernelDirectory();
}

VirtAddr VirtualMemoryManager::mapPagesAt(VirtAddr vaddr, size_t num_pages,
										  VMMPermission permission,
										  VirtAddr user_limit) {
	if (num_pages == 0) return VirtAddr();

	VirtAddr start = vaddr.pageBase();
	uint64_t end = static_cast<uint64_t>(start.raw()) +
				   static_cast<uint64_t>(num_pages) * PAGE_SIZE;
	if (end > ADDRESS_SPACE_END) return VirtAddr();
	if (permission & VMMPermission::USER) {
		if (user_limit.isNull() || user_limit > USER_SPACE_END)
			return VirtAddr();
		if (end > user_limit.raw()) return VirtAddr();
	}

	uint32_t flags = toX86Flag(permission);
	InterruptGuard guard;

	for (size_t idx = 0; idx < num_pages; idx++) {
		VirtAddr virt = start + idx * PAGE_SIZE;
		uint32_t existing = Paging::getPageFlags(virt);

		if (existing != 0) {
			uint32_t merged = existing | flags;
			if (merged != existing && !Paging::updatePageFlags(virt, merged))
				return VirtAddr();
			continue;
		}

		if (!mapFresh(virt, 1, flags)) {
			Logger::fatal("VMM: Out of memory");
			return VirtAddr();
		}
	}

	return start;
}

bool VirtualMemoryManager::isUserRangeValid(VirtAddr ptr, size_t length,
											bool write, VirtAddr user_limit) {
	if (user_limit.isNull() || user_limit > USER_SPACE_END) return false;
	if (length > ADDRESS_SPACE_END - 1) return false;
	if (ptr > user_limit) return false;
	if (static_cast<uint64_t>(ptr.raw()) + length > user_limit.raw())
		return false;
	return Paging::isRangeAccessible(ptr, length, true, write);
}

PhysAddr VirtualMemoryManager::getPhysicalAddress(VirtAddr virt) {
	return Paging::translate(virt);
}

} // namespace cock::core::memory
