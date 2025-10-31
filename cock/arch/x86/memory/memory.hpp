#ifndef MEMORY_HPP
#define MEMORY_HPP
#include "cock/arch/x86/boot/multiboot.hpp"
#include <stddef.h>
#include <stdint.h>
namespace cock::arch::x86 {

static constexpr size_t PAGE_DIR_SIZE = 0x400;
extern "C" uint32_t initial_page_dir[PAGE_DIR_SIZE];

class MemoryManager {
  public:
	static void init(MBInfo *boot_info);
	static void pmmInit(uint32_t mem_low, uint32_t mem_high);
	static void invalidatePage(uint32_t virtual_address);

  private:
	static constexpr uint32_t PAGE_SIZE = 0x1000;
	static constexpr uint32_t NUM_PAGE_DIRS = 0x100;
	static constexpr uint32_t NUM_PAGE_FRAMES = (0x100000000 / PAGE_SIZE / 8);

	static constexpr uintptr_t KERNEL_START = 0xC0000000;
	static constexpr uint32_t PAGE_FLAG_PRESENT = (1 << 0);
	static constexpr uint32_t PAGE_FLAG_WRITE = (1 << 1);

	static MBInfo *bootInfo;
	static uint32_t pageFrameMin;
	static uint32_t pageFrameMax;
	static uint32_t totalAlloc;
	static uint8_t physicalMemoryBitMap[NUM_PAGE_FRAMES / 8];
	static __attribute__((aligned(
		PAGE_SIZE))) uint32_t pageDirectories[NUM_PAGE_DIRS][PAGE_DIR_SIZE];
	static uint8_t usedPageDirectories[NUM_PAGE_DIRS];

	static void initMemory(uint32_t physicalAllocStart);
};

} // namespace cock::arch::x86

#endif // !MEMORY_HPP
