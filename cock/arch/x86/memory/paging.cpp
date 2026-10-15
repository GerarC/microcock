#include <cock/arch/x86/memory/layout.hpp>
#include <cock/arch/x86/memory/paging.hpp>
#include <cock/arch/x86/memory/pmm.hpp>
#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <hal/cpu.hpp>
#include <string.h>

extern "C" char _text_start[];
extern "C" char _text_end[];
extern "C" char _rodata_start[];
extern "C" char _rodata_end[];

namespace cock::arch::x86 {

using core::hal::InterruptGuard;
using core::memory::PhysAddr;
using core::memory::VirtAddr;
using cock::utils::Logger;
using cock::utils::panic;

namespace {

constexpr uint32_t PAGE_FLAGS_MASK = PAGE_MASK;
constexpr uint32_t TEMP_DIRECTORY_SLOT = TEMP_MAP_BASE;
constexpr uint32_t TEMP_TABLE_SLOT = TEMP_MAP_BASE + PAGE_SIZE;
constexpr uint32_t TABLE_ACCESS = PRESENT | WRITEABLE;
constexpr uint64_t LAST_ADDRESS = 0xFFFFFFFFull;

bool globalPagesActive = false;

inline uint32_t *directoryEntries() {
	return reinterpret_cast<uint32_t *>(RECURSIVE_DIRECTORY);
}

inline uint32_t *tableEntries(uint32_t directory_index) {
	return reinterpret_cast<uint32_t *>(RECURSIVE_TABLES +
										directory_index * PAGE_SIZE);
}

inline uint32_t directoryIndex(VirtAddr virt) {
	return static_cast<uint32_t>(virt.raw()) >> PAGE_TABLE_SHIFT;
}

inline uint32_t tableIndex(VirtAddr virt) {
	return (static_cast<uint32_t>(virt.raw()) >> PAGE_SHIFT) & ENTRY_INDEX_MASK;
}

inline uint32_t globalFlagFor(VirtAddr virt, uint32_t flags) {
	if (!globalPagesActive || virt.raw() < KERNEL_BASE || (flags & USER))
		return 0;
	return GLOBAL;
}

} // namespace

void Paging::init() {
	reserveKernelTables();
	enableGlobalPages();
	protectKernelImage();
}

void Paging::protectKernelImage() {
	const uintptr_t ranges[][2] = {
		{reinterpret_cast<uintptr_t>(_text_start),
		 reinterpret_cast<uintptr_t>(_text_end)},
		{reinterpret_cast<uintptr_t>(_rodata_start),
		 reinterpret_cast<uintptr_t>(_rodata_end)},
	};

	for (const auto &range : ranges) {
		for (uintptr_t page = range[0] & ~static_cast<uintptr_t>(PAGE_MASK);
			 page < range[1]; page += PAGE_SIZE) {
			VirtAddr virt(page);
			tableEntries(directoryIndex(virt))[tableIndex(virt)] &= ~WRITEABLE;
			invalidate(virt);
		}
	}

	core::hal::enable_write_protect();
	Logger::debug("Paging: kernel code and constants are read only");
}

void Paging::reserveKernelTables() {
	for (uint32_t pde = FIRST_KERNEL_PDE; pde < RECURSIVE_INDEX; pde++) {
		if (page_directory[pde] & PRESENT) continue;

		PhysAddr frame = PhysicalMemoryManager::allocFrame();
		if (frame.isNull()) panic("Paging: cannot reserve kernel page tables");

		page_directory[pde] = frame.raw() | TABLE_ACCESS;
		core::hal::invalidate_page(RECURSIVE_TABLES + pde * PAGE_SIZE);
		memset(tableEntries(pde), 0, PAGE_SIZE);
	}
}

void Paging::enableGlobalPages() {
	if (!core::hal::cpu_supports_global_pages()) {
		Logger::debug("Paging: global pages are not supported");
		return;
	}

	for (uint32_t pde = FIRST_KERNEL_PDE; pde < RECURSIVE_INDEX; pde++) {
		if (!(directoryEntries()[pde] & PRESENT)) continue;

		uint32_t *table = tableEntries(pde);
		for (uint32_t pte = 0; pte < ENTRIES_PER_TABLE; pte++) {
			if (table[pte] & PRESENT) table[pte] |= GLOBAL;
		}
	}

	core::hal::enable_global_pages();
	globalPagesActive = true;
	Logger::debug("Paging: global kernel pages enabled");
}

bool Paging::globalPagesEnabled() { return globalPagesActive; }

void Paging::invalidate(VirtAddr virt) {
	core::hal::invalidate_page(virt.raw());
}

bool Paging::mapPage(PhysAddr phys, VirtAddr virt, uint32_t flags) {
	if ((flags & USER) && virt.raw() >= USER_TOP) return false;

	uint32_t pd_idx = directoryIndex(virt);
	uint32_t pt_idx = tableIndex(virt);
	uint32_t *directory = directoryEntries();
	uint32_t *table = tableEntries(pd_idx);

	if (!(directory[pd_idx] & PRESENT)) {
		PhysAddr table_frame = PhysicalMemoryManager::allocFrame();
		if (table_frame.isNull()) return false;

		directory[pd_idx] = table_frame.raw() | TABLE_ACCESS | (flags & USER);
		core::hal::invalidate_page(reinterpret_cast<uintptr_t>(table));
		memset(table, 0, PAGE_SIZE);
	} else if (flags & USER) {
		directory[pd_idx] |= USER;
	}

	if (table[pt_idx] & PRESENT) return false;

	table[pt_idx] = phys.pageBase().raw() | (flags & PAGE_FLAGS_MASK) |
					globalFlagFor(virt, flags);
	invalidate(virt);
	return true;
}

void Paging::unmapPage(VirtAddr virt) {
	uint32_t pd_idx = directoryIndex(virt);
	if (!(directoryEntries()[pd_idx] & PRESENT)) return;

	tableEntries(pd_idx)[tableIndex(virt)] = 0;
	invalidate(virt);
}

uint32_t Paging::getPageFlags(VirtAddr virt) {
	uint32_t pd_idx = directoryIndex(virt);
	if (!(directoryEntries()[pd_idx] & PRESENT)) return 0;

	uint32_t entry = tableEntries(pd_idx)[tableIndex(virt)];
	if (!(entry & PRESENT)) return 0;
	return entry & PAGE_FLAGS_MASK;
}

bool Paging::updatePageFlags(VirtAddr virt, uint32_t flags) {
	uint32_t pd_idx = directoryIndex(virt);
	uint32_t pt_idx = tableIndex(virt);
	uint32_t *directory = directoryEntries();

	if (!(directory[pd_idx] & PRESENT)) return false;

	uint32_t *table = tableEntries(pd_idx);
	if (!(table[pt_idx] & PRESENT)) return false;
	if ((flags & USER) && virt.raw() >= USER_TOP) return false;

	if (flags & USER) directory[pd_idx] |= USER;
	table[pt_idx] = (table[pt_idx] & ~PAGE_FLAGS_MASK) |
					(flags & PAGE_FLAGS_MASK) | globalFlagFor(virt, flags);
	invalidate(virt);
	return true;
}

PhysAddr Paging::translate(VirtAddr virt) {
	uint32_t pd_idx = directoryIndex(virt);
	if (!(directoryEntries()[pd_idx] & PRESENT)) return PhysAddr();

	uint32_t entry = tableEntries(pd_idx)[tableIndex(virt)];
	if (!(entry & PRESENT)) return PhysAddr();

	return PhysAddr(entry & ~PAGE_MASK) + virt.pageOffset();
}

bool Paging::isRangeAccessible(VirtAddr virt, size_t length, bool user,
							   bool write) {
	if (length == 0) return true;

	uint64_t last = static_cast<uint64_t>(virt.raw()) + length - 1;
	if (last > LAST_ADDRESS) return false;
	if (user && last >= USER_TOP) return false;

	uint32_t first_page = static_cast<uint32_t>(virt.pageBase().raw());
	uint32_t last_page = static_cast<uint32_t>(last) & ~PAGE_MASK;
	const uint32_t *directory = directoryEntries();

	for (uint32_t page = first_page;; page += PAGE_SIZE) {
		VirtAddr current(page);
		uint32_t pd_idx = directoryIndex(current);

		uint32_t pde = directory[pd_idx];
		if (!(pde & PRESENT)) return false;
		if (user && !(pde & USER)) return false;

		uint32_t pte = tableEntries(pd_idx)[tableIndex(current)];
		if (!(pte & PRESENT)) return false;
		if (user && !(pte & USER)) return false;
		if (write && !(pte & WRITEABLE)) return false;

		if (page == last_page) break;
	}
	return true;
}

PhysAddr Paging::kernelDirectory() {
	return PhysAddr(reinterpret_cast<uint32_t>(page_directory) - KERNEL_BASE);
}

PhysAddr Paging::createAddressSpace() {
	InterruptGuard guard;

	PhysAddr directory_phys = PhysicalMemoryManager::allocFrame();
	if (directory_phys.isNull()) return PhysAddr();

	VirtAddr slot(TEMP_DIRECTORY_SLOT);
	if (!mapPage(directory_phys, slot, TABLE_ACCESS)) {
		PhysicalMemoryManager::freeFrame(directory_phys);
		return PhysAddr();
	}

	uint32_t *new_directory = slot.as<uint32_t>();
	const uint32_t *current = directoryEntries();

	memset(new_directory, 0, PAGE_SIZE);
	for (uint32_t idx = FIRST_KERNEL_PDE; idx < RECURSIVE_INDEX; idx++)
		new_directory[idx] = current[idx];
	new_directory[RECURSIVE_INDEX] = directory_phys.raw() | TABLE_ACCESS;

	unmapPage(slot);
	return directory_phys;
}

void Paging::destroyAddressSpace(PhysAddr directory_phys) {
	if (directory_phys.isNull() || directory_phys == kernelDirectory()) return;
	if (directory_phys.raw() == core::hal::read_page_table_root()) return;

	InterruptGuard guard;

	VirtAddr directory_slot(TEMP_DIRECTORY_SLOT);
	VirtAddr table_slot(TEMP_TABLE_SLOT);
	if (!mapPage(directory_phys, directory_slot, TABLE_ACCESS)) return;
	const uint32_t *directory = directory_slot.as<const uint32_t>();
	const uint32_t *table = table_slot.as<const uint32_t>();

	for (uint32_t pd_idx = 0; pd_idx < FIRST_KERNEL_PDE; pd_idx++) {
		if (!(directory[pd_idx] & PRESENT)) continue;

		PhysAddr table_phys(directory[pd_idx] & ~PAGE_MASK);
		if (!mapPage(table_phys, table_slot, TABLE_ACCESS)) continue;

		for (uint32_t pt_idx = 0; pt_idx < ENTRIES_PER_TABLE; pt_idx++) {
			if ((table[pt_idx] & PRESENT) && !(table[pt_idx] & NOT_OWNED))
				PhysicalMemoryManager::freeFrame(PhysAddr(table[pt_idx] & ~PAGE_MASK));
		}

		unmapPage(table_slot);
		PhysicalMemoryManager::freeFrame(table_phys);
	}

	unmapPage(directory_slot);
	PhysicalMemoryManager::freeFrame(directory_phys);
}

} // namespace cock::arch::x86
