#include <cock/arch/x86/memory/memory.hpp>
#include <cock/arch/x86/utils/helpers.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>
#include <string.h>

extern "C" void flush_tlb();
extern "C" void invalpg(void *addr);

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
	uint32_t physical_alloc_start = 0;

	if ((bootInfo->flags & (1 << 3)) && bootInfo->mods_count > 0) {

		uint32_t mods_virt = bootInfo->mods_address + KERNEL_START;

		uint32_t mod1 = *reinterpret_cast<uint32_t *>(mods_virt + 4);
		physical_alloc_start = (mod1 + 0xFFF) & ~0xFFF;

	} else {
		physical_alloc_start =
			0x400000;
	}

	initMemory(physical_alloc_start);
	Logger::trace("Memory Manager initializated");
}

void MemoryManager::initMemory(uint32_t physicalAllocStart) {
	uint32_t mem_high = bootInfo->mem_upper * PAGE_DIR_SIZE;
	page_directory[PAGE_DIR_SIZE - 1] =
		static_cast<uint32_t>(reinterpret_cast<uintptr_t>(page_directory) -
							  KERNEL_START) |
		PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE;
	flush_tlb();
	invalidatePage(0xFFFFF000);

	pmmInit(physicalAllocStart, mem_high);
}

void MemoryManager::invalidatePage(uint32_t virtual_address) {
	asm volatile("invlpg (%0)" ::"r"(virtual_address) : "memory");
}

void MemoryManager::pmmInit(uint32_t mem_low, uint32_t mem_high) {
	pageFrameMin = utils::ceil_div(mem_low, PAGE_SIZE);
	pageFrameMax = mem_high / PAGE_SIZE;
	totalAlloc = 0;

	memset(physicalMemoryBitMap, 0, sizeof(physicalMemoryBitMap));
}

} // namespace cock::arch::x86
