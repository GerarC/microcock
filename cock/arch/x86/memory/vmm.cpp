#include <cock/arch/x86/memory/layout.hpp>
#include <cock/arch/x86/memory/paging.hpp>
#include <cock/arch/x86/memory/pmm.hpp>
#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/data_structure/range_pool.hpp>
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

constexpr size_t MAX_FREE_KERNEL_RANGES = 128;
constexpr size_t KERNEL_RESERVED_FRAMES = 256;
constexpr size_t TABLE_MARGIN_EXTRA = 2;
constexpr uint64_t ADDRESS_SPACE_END = 0x100000000ULL;
constexpr size_t STACK_GUARD_PAGES = 1;

data_structure::RangePool<MAX_FREE_KERNEL_RANGES> kernelRanges(KERNEL_HEAP_BASE,
															   KERNEL_HEAP_LIMIT, PAGE_SIZE);
uintptr_t scratchVaddr = 0;

uint32_t toX86Flag(VMMPermission permission) {
	uint32_t arch_flags = PageFlag::PRESENT;

	if (permission & VMMPermission::WRITE) arch_flags |= PageFlag::WRITEABLE;
	if (permission & VMMPermission::USER) arch_flags |= PageFlag::USER;
	if (permission & VMMPermission::DEVICE)
		arch_flags |= PageFlag::NOT_OWNED | PageFlag::PCD;
	if (permission & VMMPermission::SHARED) arch_flags |= PageFlag::NOT_OWNED;

	return arch_flags;
}

bool reserveKernelRange(size_t pages, uintptr_t &out) { return kernelRanges.reserve(pages, out); }

bool inKernelRanges(uintptr_t address) {
	return address >= KERNEL_HEAP_BASE && address < KERNEL_HEAP_LIMIT;
}

void releaseKernelRange(uintptr_t start, size_t pages) {
	if (inKernelRanges(start)) kernelRanges.release(start, pages);
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

} // namespace

void *VirtualMemoryManager::allocPages(size_t page_count,
									   VMMPermission permission) {
	if (page_count == 0 || (permission & VMMPermission::USER)) return nullptr;

	InterruptGuard guard;
	uintptr_t start_virt = 0;
	if (!reserveKernelRange(page_count, start_virt)) {
		Logger::fatal("VMM: Out of memory");
		return nullptr;
	}
	if (!mapFresh(VirtAddr(start_virt), page_count, toX86Flag(permission))) {
		releaseKernelRange(start_virt, page_count);
		Logger::fatal("VMM: Out of memory");
		return nullptr;
	}

	return reinterpret_cast<void *>(start_virt);
}

void VirtualMemoryManager::freePages(void *vaddr, size_t page_count) {
	InterruptGuard guard;
	VirtAddr virt = VirtAddr::of(vaddr);
	releasePages(virt, page_count);
	releaseKernelRange(virt.raw(), page_count);
}

void *VirtualMemoryManager::allocStack(size_t page_count) {
	if (page_count == 0) return nullptr;

	InterruptGuard guard;
	uintptr_t region = 0;
	if (!reserveKernelRange(page_count + STACK_GUARD_PAGES, region)) return nullptr;
	uintptr_t base = region + STACK_GUARD_PAGES * PAGE_SIZE;

	if (!mapFresh(VirtAddr(base), page_count,
				  PageFlag::PRESENT | PageFlag::WRITEABLE)) {
		releaseKernelRange(region, page_count + STACK_GUARD_PAGES);
		return nullptr;
	}

	return reinterpret_cast<void *>(base);
}

void VirtualMemoryManager::freeStack(void *base, size_t page_count) {
	if (!base) return;

	InterruptGuard guard;
	VirtAddr virt = VirtAddr::of(base);
	releasePages(virt, page_count);
	releaseKernelRange(virt.raw() - STACK_GUARD_PAGES * PAGE_SIZE,
					   page_count + STACK_GUARD_PAGES);
}

uintptr_t VirtualMemoryManager::kernelRangeEnd() { return kernelRanges.end(); }

void VirtualMemoryManager::releaseBootFrames(PhysAddr start, size_t bytes) {
	uint64_t first = (static_cast<uint64_t>(start.raw()) + PAGE_MASK) >> arch::x86::PAGE_SHIFT;
	uint64_t last = (static_cast<uint64_t>(start.raw()) + bytes) >> arch::x86::PAGE_SHIFT;
	for (uint64_t frame = first; frame < last; frame++)
		PhysicalMemoryManager::releaseBootFrame(
			PhysAddr(static_cast<uint32_t>(frame << arch::x86::PAGE_SHIFT)));
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

PhysAddr VirtualMemoryManager::cloneAddressSpace(PhysAddr source) {
	return Paging::cloneAddressSpace(source, KERNEL_RESERVED_FRAMES);
}

bool VirtualMemoryManager::resolveCopyOnWrite(VirtAddr virt) {
	return Paging::resolveCopyOnWrite(virt);
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

	size_t fresh = 0;
	for (size_t idx = 0; idx < num_pages; idx++) {
		if (Paging::getPageFlags(start + idx * PAGE_SIZE) == 0) fresh++;
	}

	size_t reserve = (permission & VMMPermission::USER) ? KERNEL_RESERVED_FRAMES : 0;
	size_t needed = fresh + num_pages / arch::x86::ENTRIES_PER_TABLE +
					TABLE_MARGIN_EXTRA + reserve;
	if (needed > PhysicalMemoryManager::freeFrameCount()) {
		Logger::trace("VMM: not enough free frames to map %u pages",
					  static_cast<unsigned>(num_pages));
		return VirtAddr();
	}
	bool all_fresh = fresh == num_pages;

	for (size_t idx = 0; idx < num_pages; idx++) {
		VirtAddr virt = start + idx * PAGE_SIZE;
		uint32_t existing = Paging::getPageFlags(virt);

		if (existing != 0) {
			if ((existing & PageFlag::COPY_ON_WRITE) && (flags & PageFlag::WRITEABLE)) {
				if (!Paging::resolveCopyOnWrite(virt)) return VirtAddr();
				existing = Paging::getPageFlags(virt);
			}
			uint32_t merged = existing | flags;
			if (merged != existing && !Paging::updatePageFlags(virt, merged))
				return VirtAddr();
			continue;
		}

		if (!mapFresh(virt, 1, flags)) {
			Logger::fatal("VMM: Out of memory");
			if (all_fresh) releasePages(start, idx);
			return VirtAddr();
		}
	}

	return start;
}

bool VirtualMemoryManager::protectPage(VirtAddr virt, VMMPermission permission) {
	InterruptGuard guard;
	return Paging::updatePageFlags(virt, toX86Flag(permission));
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

bool VirtualMemoryManager::isRangeUnmapped(VirtAddr start,
											size_t page_count) {
	for (size_t idx = 0; idx < page_count; idx++) {
		if (Paging::getPageFlags(start + idx * PAGE_SIZE) != 0) return false;
	}
	return true;
}

bool VirtualMemoryManager::allocFrames(PhysAddr *frames, size_t count) {
	if (frames == nullptr || count == 0) return false;

	InterruptGuard guard;
	if (count + KERNEL_RESERVED_FRAMES > PhysicalMemoryManager::freeFrameCount())
		return false;

	if (scratchVaddr == 0 && !reserveKernelRange(1, scratchVaddr)) return false;
	VirtAddr scratch(scratchVaddr);

	for (size_t idx = 0; idx < count; idx++) {
		PhysAddr frame = PhysicalMemoryManager::allocFrame();
		if (frame.isNull() ||
			!Paging::mapPage(frame, scratch, PageFlag::PRESENT | PageFlag::WRITEABLE)) {
			if (!frame.isNull()) PhysicalMemoryManager::freeFrame(frame);
			for (size_t done = 0; done < idx; done++)
				PhysicalMemoryManager::freeFrame(frames[done]);
			return false;
		}
		memset(scratch.as<void>(), 0, PAGE_SIZE);
		Paging::unmapPage(scratch);
		frames[idx] = frame;
	}
	return true;
}

void VirtualMemoryManager::freeFrames(const PhysAddr *frames, size_t count) {
	InterruptGuard guard;
	for (size_t idx = 0; idx < count; idx++)
		PhysicalMemoryManager::freeFrame(frames[idx]);
}

bool VirtualMemoryManager::isDeviceMemory(PhysAddr phys, size_t length) {
	if (length == 0) return false;

	uint64_t last = static_cast<uint64_t>(phys.raw()) + length - 1;
	if (last >= ADDRESS_SPACE_END) return false;

	for (uint64_t frame = phys.frameNumber(); frame <= (last >> arch::x86::PAGE_SHIFT);
		 frame++) {
		if (PhysicalMemoryManager::isRam(
				PhysAddr(static_cast<uint32_t>(frame << arch::x86::PAGE_SHIFT))))
			return false;
	}
	return true;
}

PhysAddr VirtualMemoryManager::getPhysicalAddress(VirtAddr virt) {
	return Paging::translate(virt);
}

} // namespace cock::core::memory
