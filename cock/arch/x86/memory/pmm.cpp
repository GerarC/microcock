#include <cock/arch/x86/boot/boot_memory.hpp>
#include <cock/arch/x86/memory/layout.hpp>
#include <cock/arch/x86/memory/pmm.hpp>
#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <stdint.h>
#include <string.h>

extern "C" char _kernel_end[];

namespace cock::arch::x86 {

using core::hal::InterruptGuard;
using core::memory::PhysAddr;
using cock::utils::Logger;
using cock::utils::panic;

namespace {

constexpr uint32_t BITS_PER_WORD = 32;
constexpr uint32_t BITMAP_WORDS = MAX_PHYSICAL_FRAMES / BITS_PER_WORD;
constexpr uint32_t NO_FRAME = 0xFFFFFFFF;
constexpr uint32_t FULL_WORD = 0xFFFFFFFF;
constexpr uint32_t REAL_MODE_AREA_END = 0x100000;
constexpr uint32_t BOOT_STRING_SIZE = 0x100;
constexpr uint32_t MMAP_SIZE_FIELD = 4;
constexpr uint32_t MMAP_MIN_ENTRY_SIZE = 20;
constexpr uint32_t KIBIBYTE = 0x400;
constexpr unsigned HIGH_WORD_SHIFT = 32;

uint32_t frameBitmap[BITMAP_WORDS];
uint32_t ramBitmap[BITMAP_WORDS];
uint32_t allocatedBitmap[BITMAP_WORDS];
uint32_t frameLimit = 0;
uint32_t searchHint = 0;
uint32_t usedFrames = 0;
uint32_t freeFrames = 0;

inline bool testFrame(uint32_t frame) {
	return frameBitmap[frame / BITS_PER_WORD] & (1u << (frame % BITS_PER_WORD));
}

inline void setFrame(uint32_t frame) {
	frameBitmap[frame / BITS_PER_WORD] |= (1u << (frame % BITS_PER_WORD));
}

inline void markRam(uint32_t frame) {
	ramBitmap[frame / BITS_PER_WORD] |= (1u << (frame % BITS_PER_WORD));
}

inline bool testAllocated(uint32_t frame) {
	return allocatedBitmap[frame / BITS_PER_WORD] &
		   (1u << (frame % BITS_PER_WORD));
}

inline void setAllocated(uint32_t frame) {
	allocatedBitmap[frame / BITS_PER_WORD] |= (1u << (frame % BITS_PER_WORD));
}

inline void clearAllocated(uint32_t frame) {
	allocatedBitmap[frame / BITS_PER_WORD] &= ~(1u << (frame % BITS_PER_WORD));
}

inline void clearFrame(uint32_t frame) {
	frameBitmap[frame / BITS_PER_WORD] &= ~(1u << (frame % BITS_PER_WORD));
}

void releaseRange(uint64_t start, uint64_t end) {
	uint64_t first = (start + PAGE_MASK) >> PAGE_SHIFT;
	uint64_t last = end >> PAGE_SHIFT;
	if (last > MAX_PHYSICAL_FRAMES) last = MAX_PHYSICAL_FRAMES;

	for (uint64_t frame = first; frame < last; frame++) {
		clearFrame(static_cast<uint32_t>(frame));
		markRam(static_cast<uint32_t>(frame));
	}
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
		const uint8_t *cursor = static_cast<const uint8_t *>(BootMemory::map(
			PhysAddr(info->mmap_address), info->mmap_length));
		if (!cursor) panic("PMM: memory map is unreachable");
		const uint8_t *end = cursor + info->mmap_length;

		while (cursor < end) {
			const MBMemoryMapEntry *entry =
				reinterpret_cast<const MBMemoryMapEntry *>(cursor);
			if (entry->size < MMAP_MIN_ENTRY_SIZE) break;

			if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
				uint64_t base =
					(static_cast<uint64_t>(entry->address_high) << HIGH_WORD_SHIFT) |
					entry->address_low;
				uint64_t length =
					(static_cast<uint64_t>(entry->lenght_high) << HIGH_WORD_SHIFT) |
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

void reserveBootStructures(const MBInfo *info, PhysAddr info_phys) {
	reserveRange(info_phys.raw(), info_phys.raw() + sizeof(MBInfo));

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
		BootMemory::map(PhysAddr(info->mods_address), list_size));
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
		if (value == FULL_WORD) continue;

		uint32_t frame = word * BITS_PER_WORD + __builtin_ctz(~value);
		if (frame >= frameLimit) continue;

		searchHint = word;
		return frame;
	}
	return NO_FRAME;
}

} // namespace

void PhysicalMemoryManager::init(const MBInfo *boot_info, PhysAddr boot_info_phys) {
	memset(frameBitmap, 0xFF, sizeof(frameBitmap));
	memset(ramBitmap, 0, sizeof(ramBitmap));
	memset(allocatedBitmap, 0, sizeof(allocatedBitmap));
	frameLimit = 0;
	searchHint = 0;
	usedFrames = 0;

	loadMemoryMap(boot_info);

	uint32_t kernel_end_phys =
		reinterpret_cast<uint32_t>(_kernel_end) - KERNEL_BASE;
	reserveRange(0, REAL_MODE_AREA_END);
	reserveRange(0, kernel_end_phys);
	reserveBootStructures(boot_info, boot_info_phys);

	freeFrames = 0;
	for (uint32_t frame = 0; frame < frameLimit; frame++) {
		if (!testFrame(frame)) freeFrames++;
	}

	Logger::debug("Physical Memory Manager initializated (%u KiB addressable)",
				  frameLimit * (PAGE_SIZE / KIBIBYTE));
}

bool PhysicalMemoryManager::isRam(PhysAddr address) {
	size_t frame = address.frameNumber();
	if (frame >= MAX_PHYSICAL_FRAMES) return false;
	return ramBitmap[frame / BITS_PER_WORD] & (1u << (frame % BITS_PER_WORD));
}

size_t PhysicalMemoryManager::freeFrameCount() { return freeFrames; }

PhysAddr PhysicalMemoryManager::allocFrame() {
	InterruptGuard guard;

	uint32_t frame = findFreeFrame();
	if (frame == NO_FRAME) return PhysAddr();

	setFrame(frame);
	setAllocated(frame);
	usedFrames++;
	freeFrames--;
	return PhysAddr(frame * PAGE_SIZE);
}

void PhysicalMemoryManager::freeFrame(PhysAddr address) {
	InterruptGuard guard;

	uint32_t frame = static_cast<uint32_t>(address.frameNumber());
	if (frame >= frameLimit || !testAllocated(frame)) return;

	clearAllocated(frame);
	clearFrame(frame);
	freeFrames++;
	if (usedFrames > 0) usedFrames--;
	if (frame / BITS_PER_WORD < searchHint) searchHint = frame / BITS_PER_WORD;
}

} // namespace cock::arch::x86
