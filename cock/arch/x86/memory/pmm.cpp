#include <cock/arch/x86/boot/boot_memory.hpp>
#include <cock/arch/x86/memory/layout.hpp>
#include <cock/arch/x86/memory/paging.hpp>
#include <cock/arch/x86/memory/pmm.hpp>
#include <cock/arch/x86/utils/helpers.hpp>
#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <stdint.h>
#include <string.h>

extern "C" char _kernel_end[];

namespace cock::arch::x86 {

using cock::core::hal::InterruptGuard;
using cock::utils::Logger;
using cock::utils::panic;

namespace {

constexpr uint32_t BITS_PER_WORD = 32;
constexpr uint32_t BITMAP_WORDS = MAX_PHYSICAL_FRAMES / BITS_PER_WORD;
constexpr uint32_t NO_FRAME = 0xFFFFFFFF;
constexpr uint32_t REAL_MODE_AREA_END = 0x100000;
constexpr uint32_t BOOT_STRING_SIZE = 0x100;
constexpr uint32_t MMAP_SIZE_FIELD = 4;
constexpr uint32_t MMAP_MIN_ENTRY_SIZE = 20;
constexpr uint32_t KIBIBYTE = 0x400;
constexpr uint32_t PAGE_FLAGS_MASK = PAGE_MASK;
constexpr uint32_t TEMP_DIRECTORY_SLOT = TEMP_MAP_BASE;
constexpr uint32_t TEMP_TABLE_SLOT = TEMP_MAP_BASE + PAGE_SIZE;
constexpr uint32_t TABLE_ACCESS = PRESENT | WRITEABLE;

uint32_t frameBitmap[BITMAP_WORDS];
uint32_t frameLimit = 0;
uint32_t searchHint = 0;
uint32_t usedFrames = 0;

inline bool testFrame(uint32_t frame) {
	return frameBitmap[frame / BITS_PER_WORD] & (1u << (frame % BITS_PER_WORD));
}

inline void setFrame(uint32_t frame) {
	frameBitmap[frame / BITS_PER_WORD] |= (1u << (frame % BITS_PER_WORD));
}

inline void clearFrame(uint32_t frame) {
	frameBitmap[frame / BITS_PER_WORD] &= ~(1u << (frame % BITS_PER_WORD));
}

inline uint32_t *directoryEntries() {
	return reinterpret_cast<uint32_t *>(RECURSIVE_DIRECTORY);
}

inline uint32_t *tableEntries(uint32_t directory_index) {
	return reinterpret_cast<uint32_t *>(RECURSIVE_TABLES +
										directory_index * PAGE_SIZE);
}

void releaseRange(uint64_t start, uint64_t end) {
	uint64_t first = (start + PAGE_MASK) >> PAGE_SHIFT;
	uint64_t last = end >> PAGE_SHIFT;
	if (last > MAX_PHYSICAL_FRAMES) last = MAX_PHYSICAL_FRAMES;

	for (uint64_t frame = first; frame < last; frame++)
		clearFrame(static_cast<uint32_t>(frame));
	if (last > frameLimit) frameLimit = static_cast<uint32_t>(last);
}

void reserveRange(uint32_t start, uint32_t end) {
	if (end <= start) return;

	uint64_t first = start >> PAGE_SHIFT;
	uint64_t last = (static_cast<uint64_t>(end) + PAGE_MASK) >> PAGE_SHIFT;
	if (last > MAX_PHYSICAL_FRAMES) last = MAX_PHYSICAL_FRAMES;

	for (uint64_t frame = first; frame < last; frame++)
		setFrame(static_cast<uint32_t>(frame));
}

void loadMemoryMap(const MBInfo *info) {
	if (info->flags & MULTIBOOT_FLAG_MMAP) {
		const uint8_t *cursor = static_cast<const uint8_t *>(
			BootMemory::map(info->mmap_address, info->mmap_length));
		if (!cursor) panic("PMM: memory map is unreachable");
		const uint8_t *end = cursor + info->mmap_length;

		while (cursor < end) {
			const MBMemoryMapEntry *entry =
				reinterpret_cast<const MBMemoryMapEntry *>(cursor);
			if (entry->size < MMAP_MIN_ENTRY_SIZE) break;

			if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
				uint64_t base =
					(static_cast<uint64_t>(entry->address_high) << 32) |
					entry->address_low;
				uint64_t length =
					(static_cast<uint64_t>(entry->lenght_high) << 32) |
					entry->lenght_low;
				releaseRange(base, base + length);
			}
			cursor += entry->size + MMAP_SIZE_FIELD;
		}
		return;
	}

	if (info->flags & MULTIBOOT_FLAG_MEMORY) {
		uint64_t upper = static_cast<uint64_t>(info->mem_upper) * KIBIBYTE;
		releaseRange(REAL_MODE_AREA_END, REAL_MODE_AREA_END + upper);
		return;
	}

	panic("PMM: bootloader did not provide memory information");
}

void reserveBootStructures(const MBInfo *info, uint32_t info_phys) {
	reserveRange(info_phys, info_phys + sizeof(MBInfo));

	if (info->flags & MULTIBOOT_FLAG_MMAP)
		reserveRange(info->mmap_address,
					 info->mmap_address + info->mmap_length);

	if ((info->flags & MULTIBOOT_FLAG_CMDLINE) && info->cmdline)
		reserveRange(info->cmdline, info->cmdline + BOOT_STRING_SIZE);

	if (!(info->flags & MULTIBOOT_FLAG_MODULES) || info->mods_count == 0)
		return;

	uint32_t list_size = info->mods_count * sizeof(MBModuleList);
	reserveRange(info->mods_address, info->mods_address + list_size);

	const MBModuleList *modules = static_cast<const MBModuleList *>(
		BootMemory::map(info->mods_address, list_size));
	if (!modules) panic("PMM: module list is unreachable");

	for (uint32_t idx = 0; idx < info->mods_count; idx++) {
		reserveRange(modules[idx].mod_start, modules[idx].mod_end);
		if (modules[idx].cmdline)
			reserveRange(modules[idx].cmdline,
						 modules[idx].cmdline + BOOT_STRING_SIZE);
	}
}

uint32_t findFreeFrame() {
	uint32_t words = (frameLimit + BITS_PER_WORD - 1) / BITS_PER_WORD;
	if (words == 0) return NO_FRAME;

	for (uint32_t step = 0; step < words; step++) {
		uint32_t word = (searchHint + step) % words;
		uint32_t value = frameBitmap[word];
		if (value == 0xFFFFFFFF) continue;

		uint32_t frame = word * BITS_PER_WORD + __builtin_ctz(~value);
		if (frame >= frameLimit) continue;

		searchHint = word;
		return frame;
	}
	return NO_FRAME;
}

void reserveKernelTables() {
	for (uint32_t pde = FIRST_KERNEL_PDE; pde < RECURSIVE_INDEX; pde++) {
		if (page_directory[pde] & PRESENT) continue;

		uint32_t frame = PhysicalMemoryManager::allocFrame();
		if (frame == NULL_PTR) panic("PMM: cannot reserve kernel page tables");

		page_directory[pde] = frame | TABLE_ACCESS;
		PhysicalMemoryManager::invalidatePage(RECURSIVE_TABLES +
											  pde * PAGE_SIZE);
		memset(tableEntries(pde), 0, PAGE_SIZE);
	}
}

} // namespace

void PhysicalMemoryManager::init(const MBInfo *boot_info,
								 uint32_t boot_info_phys) {
	memset(frameBitmap, 0xFF, sizeof(frameBitmap));
	frameLimit = 0;
	searchHint = 0;
	usedFrames = 0;

	loadMemoryMap(boot_info);

	uint32_t kernel_end_phys =
		reinterpret_cast<uint32_t>(_kernel_end) - KERNEL_BASE;
	reserveRange(0, REAL_MODE_AREA_END);
	reserveRange(0, kernel_end_phys);
	reserveBootStructures(boot_info, boot_info_phys);

	reserveKernelTables();

	Logger::debug("Physical Memory Manager initializated (%u KiB addressable)",
				  frameLimit * (PAGE_SIZE / KIBIBYTE));
}

void PhysicalMemoryManager::invalidatePage(uint32_t virtual_address) {
	utils::invalidate_page(virtual_address);
}

uint32_t PhysicalMemoryManager::allocFrame() {
	InterruptGuard guard;

	uint32_t frame = findFreeFrame();
	if (frame == NO_FRAME) return NULL_PTR;

	setFrame(frame);
	usedFrames++;
	return frame * PAGE_SIZE;
}

void PhysicalMemoryManager::freeFrame(uint32_t phys_addr) {
	InterruptGuard guard;

	uint32_t frame = phys_addr >> PAGE_SHIFT;
	if (frame >= frameLimit || !testFrame(frame)) return;

	clearFrame(frame);
	if (usedFrames > 0) usedFrames--;
	if (frame / BITS_PER_WORD < searchHint) searchHint = frame / BITS_PER_WORD;
}

bool PhysicalMemoryManager::mapPage(uint32_t phys, uint32_t virt,
									uint32_t flags) {
	if ((flags & USER) && virt >= USER_TOP) return false;

	uint32_t pd_idx = virt >> PAGE_TABLE_SHIFT;
	uint32_t pt_idx = (virt >> PAGE_SHIFT) & ENTRY_INDEX_MASK;
	uint32_t *directory = directoryEntries();
	uint32_t *table = tableEntries(pd_idx);

	if (!(directory[pd_idx] & PRESENT)) {
		uint32_t pt_phys = allocFrame();
		if (pt_phys == NULL_PTR) return false;

		directory[pd_idx] = pt_phys | TABLE_ACCESS | (flags & USER);
		invalidatePage(reinterpret_cast<uint32_t>(table));
		memset(table, 0, PAGE_SIZE);
	} else if (flags & USER) {
		directory[pd_idx] |= USER;
	}

	if (table[pt_idx] & PRESENT) return false;

	table[pt_idx] = (phys & ~PAGE_MASK) | (flags & PAGE_FLAGS_MASK);
	invalidatePage(virt);
	return true;
}

void PhysicalMemoryManager::unmapPage(uint32_t virt) {
	uint32_t pd_idx = virt >> PAGE_TABLE_SHIFT;
	uint32_t pt_idx = (virt >> PAGE_SHIFT) & ENTRY_INDEX_MASK;

	if (!(directoryEntries()[pd_idx] & PRESENT)) return;

	tableEntries(pd_idx)[pt_idx] = 0;
	invalidatePage(virt);
}

uint32_t PhysicalMemoryManager::getPageFlags(uint32_t virt) {
	uint32_t pd_idx = virt >> PAGE_TABLE_SHIFT;
	uint32_t pt_idx = (virt >> PAGE_SHIFT) & ENTRY_INDEX_MASK;

	if (!(directoryEntries()[pd_idx] & PRESENT)) return 0;

	uint32_t entry = tableEntries(pd_idx)[pt_idx];
	if (!(entry & PRESENT)) return 0;
	return entry & PAGE_FLAGS_MASK;
}

bool PhysicalMemoryManager::updatePageFlags(uint32_t virt, uint32_t flags) {
	uint32_t pd_idx = virt >> PAGE_TABLE_SHIFT;
	uint32_t pt_idx = (virt >> PAGE_SHIFT) & ENTRY_INDEX_MASK;
	uint32_t *directory = directoryEntries();

	if (!(directory[pd_idx] & PRESENT)) return false;

	uint32_t *table = tableEntries(pd_idx);
	if (!(table[pt_idx] & PRESENT)) return false;
	if ((flags & USER) && virt >= USER_TOP) return false;

	if (flags & USER) directory[pd_idx] |= USER;
	table[pt_idx] =
		(table[pt_idx] & ~PAGE_FLAGS_MASK) | (flags & PAGE_FLAGS_MASK);
	invalidatePage(virt);
	return true;
}

uint32_t PhysicalMemoryManager::getPhysicalAddress(uint32_t virt) {
	uint32_t pd_idx = virt >> PAGE_TABLE_SHIFT;
	uint32_t pt_idx = (virt >> PAGE_SHIFT) & ENTRY_INDEX_MASK;

	if (!(directoryEntries()[pd_idx] & PRESENT)) return NULL_PTR;

	uint32_t entry = tableEntries(pd_idx)[pt_idx];
	if (!(entry & PRESENT)) return NULL_PTR;

	return (entry & ~PAGE_MASK) + (virt & PAGE_MASK);
}

bool PhysicalMemoryManager::isRangeAccessible(uint32_t virt, uint32_t length,
											  bool user, bool write) {
	if (length == 0) return true;

	uint64_t last = static_cast<uint64_t>(virt) + length - 1;
	if (last > 0xFFFFFFFFull) return false;
	if (user && last >= USER_TOP) return false;

	uint32_t first_page = virt & ~PAGE_MASK;
	uint32_t last_page = static_cast<uint32_t>(last) & ~PAGE_MASK;
	const uint32_t *directory = directoryEntries();

	for (uint32_t page = first_page;; page += PAGE_SIZE) {
		uint32_t pd_idx = page >> PAGE_TABLE_SHIFT;
		uint32_t pt_idx = (page >> PAGE_SHIFT) & ENTRY_INDEX_MASK;

		uint32_t pde = directory[pd_idx];
		if (!(pde & PRESENT)) return false;
		if (user && !(pde & USER)) return false;

		uint32_t pte = tableEntries(pd_idx)[pt_idx];
		if (!(pte & PRESENT)) return false;
		if (user && !(pte & USER)) return false;
		if (write && !(pte & WRITEABLE)) return false;

		if (page == last_page) break;
	}
	return true;
}

uint32_t PhysicalMemoryManager::getKernelDirectory() {
	return reinterpret_cast<uint32_t>(page_directory) - KERNEL_BASE;
}

uint32_t PhysicalMemoryManager::createAddressSpace() {
	InterruptGuard guard;

	uint32_t directory_phys = allocFrame();
	if (directory_phys == NULL_PTR) return NULL_PTR;

	if (!mapPage(directory_phys, TEMP_DIRECTORY_SLOT, TABLE_ACCESS)) {
		freeFrame(directory_phys);
		return NULL_PTR;
	}

	uint32_t *new_directory =
		reinterpret_cast<uint32_t *>(TEMP_DIRECTORY_SLOT);
	const uint32_t *current = directoryEntries();

	memset(new_directory, 0, PAGE_SIZE);
	for (uint32_t idx = FIRST_KERNEL_PDE; idx < RECURSIVE_INDEX; idx++)
		new_directory[idx] = current[idx];
	new_directory[RECURSIVE_INDEX] = directory_phys | TABLE_ACCESS;

	unmapPage(TEMP_DIRECTORY_SLOT);
	return directory_phys;
}

void PhysicalMemoryManager::destroyAddressSpace(uint32_t pd_phys) {
	if (pd_phys == NULL_PTR || pd_phys == getKernelDirectory()) return;
	if (pd_phys == utils::read_cr3()) return;

	InterruptGuard guard;

	if (!mapPage(pd_phys, TEMP_DIRECTORY_SLOT, TABLE_ACCESS)) return;
	const uint32_t *directory =
		reinterpret_cast<const uint32_t *>(TEMP_DIRECTORY_SLOT);
	const uint32_t *table = reinterpret_cast<const uint32_t *>(TEMP_TABLE_SLOT);

	for (uint32_t pd_idx = 0; pd_idx < FIRST_KERNEL_PDE; pd_idx++) {
		if (!(directory[pd_idx] & PRESENT)) continue;

		uint32_t table_phys = directory[pd_idx] & ~PAGE_MASK;
		if (!mapPage(table_phys, TEMP_TABLE_SLOT, TABLE_ACCESS)) continue;

		for (uint32_t pt_idx = 0; pt_idx < ENTRIES_PER_TABLE; pt_idx++) {
			if (table[pt_idx] & PRESENT) freeFrame(table[pt_idx] & ~PAGE_MASK);
		}

		unmapPage(TEMP_TABLE_SLOT);
		freeFrame(table_phys);
	}

	unmapPage(TEMP_DIRECTORY_SLOT);
	freeFrame(pd_phys);
}

} // namespace cock::arch::x86
