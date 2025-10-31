#include <cock/arch/x86/memory/memory.hpp>
#include <cock/arch/x86/utils/helpers.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::arch::x86 {
using cock::utils::Logger;
constexpr uint32_t PAGE_SIZE = 0x1000;

// Initialization
MBInfo *MemoryManager::bootInfo = nullptr;
uint32_t MemoryManager::pageFrameMin;
uint32_t MemoryManager::pageFrameMax;
uint32_t MemoryManager::totalAlloc;
uint8_t MemoryManager::physicalMemoryBitMap[NUM_PAGE_FRAMES / 8];
__attribute__((aligned(PAGE_SIZE))) uint32_t
	MemoryManager::pageDirectories[NUM_PAGE_DIRS][PAGE_DIR_SIZE];
uint8_t MemoryManager::usedPageDirectories[NUM_PAGE_DIRS];

void MemoryManager::init(MBInfo *boot_info) {
	bootInfo = boot_info;

	uint32_t mod1 = *reinterpret_cast<uint32_t *>((bootInfo->mods_address + 4));
	uint32_t physical_alloc_start = (mod1 + 0xFFF) & ~0xFFF;
	initMemory(physical_alloc_start);
	Logger::trace("Memory Manager initializated");
}

void MemoryManager::initMemory(uint32_t physicalAllocStart) {
	uint32_t mem_high = bootInfo->mem_upper * PAGE_DIR_SIZE;
	initial_page_dir[0] = 0;
	invalidatePage(0);
	initial_page_dir[PAGE_DIR_SIZE - 1] =
		static_cast<uint32_t>(reinterpret_cast<uintptr_t>(initial_page_dir) -
							  KERNEL_START) |
		PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE;
	invalidatePage(0xFFFFF000);

	pmmInit(physicalAllocStart, mem_high);
	memset(pageDirectories, 0, NUM_PAGE_DIRS * PAGE_SIZE);
	memset(usedPageDirectories, 0, NUM_PAGE_DIRS);
}

void MemoryManager::invalidatePage(uint32_t virtual_address) {
	asm volatile("invlpg %0" ::"m"(virtual_address));
}

void MemoryManager::pmmInit(uint32_t mem_low, uint32_t mem_high) {
	pageFrameMin = utils::ceil_div(mem_low, PAGE_SIZE);
	pageFrameMax = mem_high / PAGE_SIZE;
	totalAlloc = 0;

	memset(physicalMemoryBitMap, 0, sizeof(physicalMemoryBitMap));
}

} // namespace cock::arch::x86
